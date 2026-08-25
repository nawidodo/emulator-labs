// nesboot.hpp — headless NROM boot rig for the challenge.
//
// Combines the exercise-01 bus decode, the exercise-02 cartridge stack and
// a deliberately small 6502 execution core. The course-original boot ROMs
// use a documented instruction subset (see kSupported note above step());
// anything outside it halts the core so rigs fail loudly, not silently.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace nesboot {

// ---- cartridge layer ------------------------------------------------------

enum class Mirroring : uint8_t { Horizontal, Vertical, FourScreen };

struct Header {
    uint8_t prg_banks = 0;
    uint8_t chr_banks = 0;
    uint8_t mapper = 0;
    Mirroring mirroring = Mirroring::Horizontal;

    bool parse(const std::vector<uint8_t>& rom) {
        if (rom.size() < 16) return false;
        if (rom[0] != 'N' || rom[1] != 'E' || rom[2] != 'S' || rom[3] != 0x1A)
            return false;
        prg_banks = rom[4];
        chr_banks = rom[5];
        const uint8_t f6 = rom[6];
        mirroring = (f6 & 0x01) ? Mirroring::Vertical : Mirroring::Horizontal;
        if (f6 & 0x08) mirroring = Mirroring::FourScreen;
        mapper = static_cast<uint8_t>((rom[7] & 0xF0) | (f6 >> 4));
        return true;
    }
};

struct Mapper {
    virtual ~Mapper() = default;
    virtual uint8_t cpu_read(uint16_t addr) = 0;
    virtual void cpu_write(uint16_t addr, uint8_t v) = 0;
};

class NROM final : public Mapper {
public:
    std::vector<uint8_t> prg;
    Mirroring mirroring = Mirroring::Horizontal;

    uint8_t cpu_read(uint16_t addr) override {
        if (addr < 0x8000) return 0;
        return prg[(addr - 0x8000) % prg.size()];
    }
    void cpu_write(uint16_t, uint8_t) override {}

    static NROM* create(const Header& h, const std::vector<uint8_t>& rom) {
        auto* cart = new NROM();
        const size_t skip = 16 + ((rom[6] & 0x04) ? 512 : 0);
        cart->prg.assign(rom.begin() + long(skip),
                         rom.begin() + long(skip + size_t(h.prg_banks) * 16384));
        cart->mirroring = h.mirroring;
        return cart;
    }
};

// ---- CPU bus --------------------------------------------------------------

class Bus {
public:
    Mapper* cartridge = nullptr;
    uint64_t cycles = 0;
    std::array<uint8_t, 0x800> ram{};   // exposed for headless assertions

    uint8_t read(uint16_t addr) {
        ++cycles;
        if (addr < 0x2000) return ram[addr & 0x07FF];
        if (cartridge != nullptr && addr >= 0x8000)
            return cartridge->cpu_read(addr);
        return 0;
    }

    void write(uint16_t addr, uint8_t v) {
        ++cycles;
        if (addr < 0x2000) {
            ram[addr & 0x07FF] = v;
        } else if (cartridge != nullptr && addr >= 0x8000) {
            cartridge->cpu_write(addr, v);
        }
    }
};

// ---- minimal 6502 ---------------------------------------------------------

enum FlagBits : uint8_t { FC = 0x01, FZ = 0x02, FI = 0x04, FU = 0x20 };

struct Cpu {
    uint8_t a = 0, x = 0, y = 0;
    uint8_t sp = 0xFD;
    uint8_t p = FU | FI;
    uint16_t pc = 0;
    uint64_t cycles = 0;
    Bus* bus = nullptr;
    bool halted = false;

    // Every CPU cycle flows through these helpers so the boot budget and
    // the runner trace see real progress even for pure-memory instructions.
    uint8_t read(uint16_t addr) { ++cycles; return bus->read(addr); }
    void write(uint16_t addr, uint8_t v) { ++cycles; bus->write(addr, v); }
    uint8_t fetch8() { return read(pc++); }
    uint16_t fetch16() {
        const uint16_t lo = fetch8();
        return static_cast<uint16_t>(lo | uint16_t(fetch8()) << 8);
    }
};

/// Hardware-style reset through the bus: S -= 3, I set, PC from $FFFC.
/// Works because the cartridge answers $FFFC through the PRG mapping.
inline void reset(Cpu& c) {
    c.sp = static_cast<uint8_t>(c.sp - 3);
    c.p = static_cast<uint8_t>((c.p & ~(FC)) | FI | FU);
    const uint16_t lo = c.read(0xFFFC);
    c.pc = static_cast<uint16_t>(lo | uint16_t(c.read(0xFFFD)) << 8);
}

//@LABS-BEGIN 1
//@LABS-SOLUTION
/// Execute one instruction of the boot subset and return cycles billed.
///
/// Supported (everything a course-original init block needs):
///   SEI CLI CLD SED NOP          implied
///   LDX # / LDY # / LDA #        immediate
///   TXS TSX TAX INX INY DEY      register ops
///   LDA zp/abs, STA zp/abs       memory moves
///   INC zp/abs                   RMW with the old-value dummy write
///   JMP abs                      control flow
/// Any other opcode HALTS the core (JAM) — boot rigs must stop loudly.
inline int step(Cpu& c) {
    if (c.halted) return 0;
    const uint64_t t0 = c.cycles;
    const uint8_t op = c.fetch8();
    auto zn = [&](uint8_t v) {
        c.p = static_cast<uint8_t>((c.p & ~(FZ)) | (v == 0 ? FZ : 0));
    };
    switch (op) {
        case 0x78: ++c.cycles; c.p |= FI; break;                 // SEI
        case 0x58: ++c.cycles; c.p &= static_cast<uint8_t>(~FI); break;  // CLI
        case 0xD8: case 0xF8: case 0xEA:                         // CLD SED NOP
            ++c.cycles;
            break;
        case 0xA2: c.x = c.fetch8(); zn(c.x); break;             // LDX #
        case 0xA0: c.y = c.fetch8(); zn(c.y); break;             // LDY #
        case 0xA9: c.a = c.fetch8(); zn(c.a); break;             // LDA #
        case 0x9A: ++c.cycles; c.sp = c.x; break;                // TXS
        case 0xBA: ++c.cycles; c.x = c.sp; zn(c.x); break;       // TSX
        case 0xAA: ++c.cycles; c.x = c.a; zn(c.x); break;        // TAX
        case 0xE8: ++c.cycles; ++c.x; zn(c.x); break;            // INX
        case 0xC8: ++c.cycles; ++c.y; zn(c.y); break;            // INY
        case 0x88: ++c.cycles; --c.y; zn(c.y); break;            // DEY
        case 0x85: {                                             // STA zp
            const uint8_t z = c.fetch8();
            c.write(z, c.a);
            break;
        }
        case 0x8D: {                                             // STA abs
            const uint16_t a = c.fetch16();
            c.write(a, c.a);
            break;
        }
        case 0xA5: {                                             // LDA zp
            const uint8_t z = c.fetch8();
            c.a = c.read(z); zn(c.a);
            break;
        }
        case 0xAD: {                                             // LDA abs
            const uint16_t a = c.fetch16();
            c.a = c.read(a); zn(c.a);
            break;
        }
        case 0xE6: {                                             // INC zp
            const uint8_t z = c.fetch8();
            const uint8_t old = c.read(z);
            c.write(z, old);                      // RMW dummy write-back
            const uint8_t nv = static_cast<uint8_t>(old + 1);
            c.write(z, nv);
            zn(nv);
            break;
        }
        case 0x4C: c.pc = c.fetch16(); break;                    // JMP abs
        default:
            c.halted = true;                                     // unknown/JAM
            break;
    }
    return static_cast<int>(c.cycles - t0);
}
//@LABS-STUB
// TODO(1): implement the boot-subset dispatcher. Implied ops bill one
// extra internal cycle beyond the opcode fetch; immediates fetch their
// operand; STA/LDA go through the bus (zp or abs); INC performs the RMW
// dance (read, write OLD value, write new); JMP loads a new PC from two
// operand bytes; ANY other opcode sets halted and stops the rig.
inline int step(Cpu& c) {
    if (c.halted) return 0;
    const uint8_t op = c.fetch8();
    switch (op) {
        case 0xEA:  // NOP is done for you as the example row
            ++c.cycles;
            break;
        default:
            c.halted = true;  // TODO(1)
            break;
    }
    return 1;
}
//@LABS-END

/// Run a loaded cartridge: reset through the vector, then execute until
/// halt or the cycle budget runs out.
inline void boot(Bus& bus, Cpu& cpu, uint64_t max_cycles) {
    reset(cpu);
    while (!cpu.halted && cpu.cycles < max_cycles) step(cpu);
}

/// Load an iNES image onto the bus. Returns false on malformed input or a
/// mapper we do not implement (this chapter: mapper 0 only).
inline bool load_ines(Bus& bus, const std::vector<uint8_t>& rom) {
    Header h;
    if (!h.parse(rom)) return false;
    if (h.mapper != 0) return false;
    bus.cartridge = NROM::create(h, rom);
    return bus.cartridge != nullptr;
}

}  // namespace nesboot
