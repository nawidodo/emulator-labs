// nesdbg.hpp — debugging exercise: bus + iNES/NROM with two seeded bugs.
//
// Self-contained copy of exercises 01/02 hardware (same code, minus the
// teaching markers). Someone "simplified" two spots; see DEBUGGING.md.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace nesbus {

class Controller {
public:
    enum Buttons : uint8_t {
        BtnA = 0x01, BtnB = 0x02, Select = 0x04, Start = 0x08,
        Up = 0x10, Down = 0x20, Left = 0x40, Right = 0x80,
    };

    void set_buttons(uint8_t pressed) { buttons_ = pressed; }

    void write_strobe(uint8_t val) {
        const bool high = (val & 0x01) != 0;
        if (high) shifter_ = buttons_;
        strobe_ = high;
    }

    uint8_t read() {
        if (strobe_) shifter_ = buttons_;
        const uint8_t bit = shifter_ & 0x01;
        if (!strobe_) shifter_ = static_cast<uint8_t>((shifter_ >> 1) | 0x80);
        return static_cast<uint8_t>(bit | 0x40);
    }

private:
    uint8_t buttons_ = 0;
    uint8_t shifter_ = 0;
    bool strobe_ = false;
};

struct PpuRegs {
    std::array<uint8_t, 8> r{};
};

struct Mapper {
    virtual ~Mapper() = default;
    virtual uint8_t cpu_read(uint16_t addr) = 0;
    virtual void cpu_write(uint16_t addr, uint8_t v) = 0;
};

class NesBus {
public:
    PpuRegs ppu;
    Controller controller[2];
    std::array<uint8_t, 256> oam{};
    uint64_t cycles = 0;
    Mapper* cartridge = nullptr;

    uint8_t ram_read(uint16_t addr) const { return ram_[addr & 0x07FF]; }
    void ram_write(uint16_t addr, uint8_t v) { ram_[addr & 0x07FF] = v; }
    uint8_t ppu_read(uint16_t addr) { return ppu.r[addr & 0x0007]; }
    void ppu_write(uint16_t addr, uint8_t v) { ppu.r[addr & 0x0007] = v; }
    uint8_t controller_read(uint16_t addr) {
        return controller[addr & 0x0001].read();
    }

//@LABS-BEGIN 2
//@LABS-SOLUTION
    // Authoritative debit: exactly 513 cycles from operation start, plus
    // one get cycle when the operation began on an odd CPU cycle.
    void trigger_oam_dma(uint8_t page) {
        const uint64_t t0 = cycles;
        const uint16_t base = static_cast<uint16_t>(page << 8);
        for (uint16_t i = 0; i < 256; ++i)
            oam[i] = read(base + i);
        cycles = t0 + 513 + (t0 & 0x01);
    }
//@LABS-STUB
    // TODO(2): BUG — this version ignores the odd/even alignment and
    // always charges exactly 513, so DMAs started on an odd cycle run one
    // cycle short of the official count.
    void trigger_oam_dma(uint8_t page) {
        const uint64_t t0 = cycles;
        const uint16_t base = static_cast<uint16_t>(page << 8);
        for (uint16_t i = 0; i < 256; ++i)
            oam[i] = read(base + i);
        cycles = t0 + 513;  // BUG: alignment get-cycle dropped
    }
//@LABS-END

    uint8_t read(uint16_t addr) {
        ++cycles;
        if (addr < 0x2000) return ram_read(addr);
        if (addr < 0x4000) return ppu_read(addr);
        if (addr == 0x4016 || addr == 0x4017) return controller_read(addr);
        if (cartridge != nullptr && addr >= 0x8000)
            return cartridge->cpu_read(addr);
        return 0;
    }

    void write(uint16_t addr, uint8_t v) {
        ++cycles;
        if (addr < 0x2000) {
            ram_write(addr, v);
        } else if (addr < 0x4000) {
            ppu_write(addr, v);
        } else if (addr == 0x4014) {
            trigger_oam_dma(v);
        } else if (addr == 0x4016 || addr == 0x4017) {
            controller[(addr - 0x4016) & 1].write_strobe(v);
        } else if (cartridge != nullptr && addr >= 0x8000) {
            cartridge->cpu_write(addr, v);
        }
    }

private:
    std::array<uint8_t, 0x800> ram_{};
};

}  // namespace nesbus

namespace nesrom {

enum class Mirroring : uint8_t { Horizontal, Vertical, FourScreen };

struct Header {
    uint8_t prg_banks = 0;
    uint8_t chr_banks = 0;
    uint8_t mapper = 0;
    Mirroring mirroring = Mirroring::Horizontal;
    bool battery = false;
    bool trainer = false;

//@LABS-BEGIN 1
//@LABS-SOLUTION
    bool parse(const std::vector<uint8_t>& rom) {
        if (rom.size() < 16) return false;
        if (rom[0] != 'N' || rom[1] != 'E' || rom[2] != 'S' || rom[3] != 0x1A)
            return false;
        prg_banks = rom[4];
        chr_banks = rom[5];
        const uint8_t f6 = rom[6];
        const uint8_t f7 = rom[7];
        // flag6 bit0: 0 -> horizontal arrangement, 1 -> vertical.
        mirroring = (f6 & 0x01) ? Mirroring::Vertical : Mirroring::Horizontal;
        battery = (f6 & 0x02) != 0;
        trainer = (f6 & 0x04) != 0;
        if (f6 & 0x08) mirroring = Mirroring::FourScreen;
        mapper = static_cast<uint8_t>((f7 & 0xF0) | (f6 >> 4));
        return true;
    }
//@LABS-STUB
    // TODO(1): BUG — this version reads the mirroring bit BACKWARDS, so
    // every game whose background is built for vertical arrangement plays
    // on a horizontally-mirrored nametable layout (and vice versa).
    bool parse(const std::vector<uint8_t>& rom) {
        if (rom.size() < 16) return false;
        if (rom[0] != 'N' || rom[1] != 'E' || rom[2] != 'S' || rom[3] != 0x1A)
            return false;
        prg_banks = rom[4];
        chr_banks = rom[5];
        const uint8_t f6 = rom[6];
        const uint8_t f7 = rom[7];
        mirroring = (f6 & 0x01) ? Mirroring::Horizontal   // BUG: inverted
                                : Mirroring::Vertical;
        battery = (f6 & 0x02) != 0;
        trainer = (f6 & 0x04) != 0;
        if (f6 & 0x08) mirroring = Mirroring::FourScreen;
        mapper = static_cast<uint8_t>((f7 & 0xF0) | (f6 >> 4));
        return true;
    }
//@LABS-END
};

inline uint16_t mirror_translate(uint16_t addr, Mirroring m) {
    addr &= 0x0FFF;
    const uint16_t table = addr >> 10;
    const uint16_t offset = addr & 0x03FF;
    switch (m) {
        case Mirroring::Vertical:
            return static_cast<uint16_t>((table & 0x01) * 0x400 + offset);
        case Mirroring::Horizontal:
            return static_cast<uint16_t>((table >> 1) * 0x400 + offset);
        case Mirroring::FourScreen:
            return static_cast<uint16_t>(table * 0x400 + offset);
    }
    return offset;
}

class NROM final : public nesbus::Mapper {
public:
    std::vector<uint8_t> prg;
    std::vector<uint8_t> chr;
    Mirroring mirroring = Mirroring::Horizontal;
    std::array<uint8_t, 0x800> ciram{};

    uint8_t cpu_read(uint16_t addr) override {
        if (addr < 0x8000) return 0;
        return prg[(addr - 0x8000) % prg.size()];
    }
    void cpu_write(uint16_t, uint8_t) override {}
    uint8_t ppu_read(uint16_t addr) {
        if (addr < 0x2000) return chr.empty() ? 0 : chr[addr & (chr.size() - 1)];
        return ciram[mirror_translate(addr, mirroring)];
    }
    void ppu_write(uint16_t addr, uint8_t v) {
        if (addr < 0x2000) return;
        ciram[mirror_translate(addr, mirroring)] = v;
    }

    static NROM* create(const Header& h, const std::vector<uint8_t>& rom) {
        auto* cart = new NROM();
        const size_t header_len = 16 + (h.trainer ? 512 : 0);
        const size_t prg_bytes = size_t(h.prg_banks) * 16384;
        const size_t chr_bytes = size_t(h.chr_banks) * 8192;
        if (rom.size() < header_len + prg_bytes + chr_bytes) {
            delete cart;
            return nullptr;
        }
        cart->prg.assign(rom.begin() + long(header_len),
                         rom.begin() + long(header_len + prg_bytes));
        cart->chr.assign(rom.begin() + long(header_len + prg_bytes),
                         rom.begin() + long(header_len + prg_bytes + chr_bytes));
        cart->mirroring = h.mirroring;
        return cart;
    }
};

}  // namespace nesrom
