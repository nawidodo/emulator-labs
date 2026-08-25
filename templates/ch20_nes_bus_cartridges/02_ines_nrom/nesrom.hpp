// nesrom.hpp — iNES cartridge: header parsing, Mapper interface, NROM.
//
// Everything above $4017 belongs to the cartridge. The connector is the
// Mapper interface; NROM (mapper 0) is the baseline: PRG ROM at $8000
// (16KB variants mirrored at $C000), CHR at PPU $0000-$1FFF, and a
// hard-wired nametable mirroring taken from the header.
//
// iNES header (16 bytes):
//   0-3   "NES\x1a"
//   4     PRG-ROM size in 16KB units
//   5     CHR-ROM size in 8KB units (0 may mean CHR-RAM — see 99)
//   6     bit0 mirroring (0 = horizontal, 1 = vertical)
//         bit1 battery, bit2 trainer (512 bytes after the header),
//         bit3 four-screen VRAM
//   7     upper nibble: mapper number bits 8-11 (we keep bits 4-7)
//   mapper = (byte7 & 0xF0) | (byte6 >> 4)
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace nesrom {

enum class Mirroring : uint8_t { Horizontal, Vertical, FourScreen };

struct Header {
    uint8_t prg_banks = 0;      // 16KB units
    uint8_t chr_banks = 0;      // 8KB units
    uint8_t mapper = 0;
    Mirroring mirroring = Mirroring::Horizontal;
    bool battery = false;
    bool trainer = false;

//@LABS-BEGIN 1
//@LABS-SOLUTION
    // Parse the 16-byte header. Returns false unless the magic matches and
    // there is at least a header's worth of data. `rom` is the WHOLE file;
    // a trailing trainer would follow bytes 16-31 but lives in the file,
    // not in any memory bank.
    bool parse(const std::vector<uint8_t>& rom) {
        if (rom.size() < 16) return false;
        if (rom[0] != 'N' || rom[1] != 'E' || rom[2] != 'S' || rom[3] != 0x1A)
            return false;
        prg_banks = rom[4];
        chr_banks = rom[5];
        const uint8_t f6 = rom[6];
        const uint8_t f7 = rom[7];
        // Bit0 of flag6 names the HARD-WIRED arrangement:
        //   0 -> horizontal arrangement (CIRAM A10 = PPU A11)
        //   1 -> vertical arrangement   (CIRAM A10 = PPU A10)
        mirroring = (f6 & 0x01) ? Mirroring::Vertical : Mirroring::Horizontal;
        battery = (f6 & 0x02) != 0;
        trainer = (f6 & 0x04) != 0;
        if (f6 & 0x08) mirroring = Mirroring::FourScreen;
        mapper = static_cast<uint8_t>((f7 & 0xF0) | (f6 >> 4));
        return true;
    }
//@LABS-STUB
    // TODO(1): fill every field from the 16-byte header. Magic "NES\x1a",
    // PRG/CHR bank counts from bytes 4/5, mirroring from flag6 bit0 (0 =
    // horizontal, 1 = vertical; flag6 bit3 forces FourScreen), battery and
    // trainer flags from flag6, and the mapper low nibble from flag6>>4
    // ORed with flag7's high nibble. Return false on bad size or magic.
    bool parse(const std::vector<uint8_t>& rom) {
        if (rom.size() < 16) return false;
        prg_banks = rom[4];  // TODO(1)
        return true;
    }
//@LABS-END
};

// The cartridge connector. The bus talks to whatever is plugged in.
struct Mapper {
    virtual ~Mapper() = default;
    virtual uint8_t cpu_read(uint16_t addr) = 0;
    virtual void cpu_write(uint16_t addr, uint8_t v) = 0;
    // PPU-side lines (CHR + nametable wiring).
    virtual uint8_t ppu_read(uint16_t addr) = 0;
    virtual void ppu_write(uint16_t addr, uint8_t v) = 0;
};

// CIRAM layout: translate a PPU nametable address ($2000-$2FFF) onto the
// onboard 2KB RAM. Horizontal mirroring stacks top/bottom halves on the
// same 1KB banks (PPU A11 picks); vertical puts left/right side by side
// (PPU A10 picks). $3000-$3EFF mirrors $2000-$2EFF.
inline uint16_t mirror_translate(uint16_t addr, Mirroring m) {
    addr &= 0x0FFF;                       // within either nametable range
    const uint16_t table = addr >> 10;    // which of the four 1KB tables
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

class NROM final : public Mapper {
public:
    std::vector<uint8_t> prg;             // 16KB or 32KB
    std::vector<uint8_t> chr;             // 8KB (empty => CHR-RAM, see 99)
    Mirroring mirroring = Mirroring::Horizontal;
    std::array<uint8_t, 0x800> ciram{};   // onboard 2KB nametable RAM

//@LABS-BEGIN 2
//@LABS-SOLUTION
    // $8000-$FFFF answers PRG. With one 16KB bank the machine can't leave
    // any address unmapped, so the bank appears TWICE: $8000-$BFFF and its
    // mirror at $C000-$FFFF. 32KB maps linearly.
    uint8_t cpu_read(uint16_t addr) override {
        if (addr < 0x8000) return 0;
        const uint16_t off = static_cast<uint16_t>((addr - 0x8000) %
                                                   prg.size());
        return prg[off];
    }

    void cpu_write(uint16_t, uint8_t) override {
        // NROM has no writable PRG hardware; writes are silently dropped.
    }
//@LABS-STUB
    // TODO(2): map $8000-$FFFF onto PRG. For a single-bank (16KB) cart the
    // bank must appear at BOTH $8000-$BFFF AND $C000-$FFFF — the reset
    // vector lives at $FFFC even when the cart only has 16KB! Writes have
    // nowhere to go on NROM.
    uint8_t cpu_read(uint16_t addr) override {
        if (addr < 0x8000 || addr - 0x8000 >= prg.size()) return 0;  // TODO(2)
        return prg[addr - 0x8000];
    }

    void cpu_write(uint16_t, uint8_t) override {}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
    // PPU side: $0000-$1FFF is CHR; $2000+ lands in the onboard CIRAM via
    // the cartridge's hard-wired mirroring.
    uint8_t ppu_read(uint16_t addr) override {
        if (addr < 0x2000) {
            return chr.empty() ? 0 : chr[addr & (chr.size() - 1)];
        }
        return ciram[mirror_translate(addr, mirroring)];
    }

    void ppu_write(uint16_t addr, uint8_t v) override {
        if (addr < 0x2000) return;    // CHR ROM is not writable
        ciram[mirror_translate(addr, mirroring)] = v;
    }
//@LABS-STUB
    // TODO(3): route PPU addresses. Below $2000: CHR space (ROM here).
    // At or above $2000: the onboard CIRAM at mirror_translate(addr,
    // mirroring). Writes to CHR ROM are dropped; writes to CIRAM stick.
    uint8_t ppu_read(uint16_t addr) override {
        if (addr < 0x2000) return 0;  // TODO(3)
        return ciram[0];
    }

    void ppu_write(uint16_t, uint8_t) override {}  // TODO(3)
//@LABS-END

    // Build a cartridge from a parsed iNES file (skipping the 16-byte
    // header and optional 512-byte trainer). Returns nullptr on mismatch.
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
