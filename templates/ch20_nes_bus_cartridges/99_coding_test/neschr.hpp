// neschr.hpp — coding test support: mapper-zero variant with CHR-RAM.
//
// Identical to exercise 02's stack except one degree of freedom: an iNES
// file whose CHR bank count is ZERO has no CHR ROM at all — the cartridge
// carries 8KB of writable CHR-RAM instead. Your job (see main.cpp) is the
// PPU-side read/write routing for that variant.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace neschr {

enum class Mirroring : uint8_t { Horizontal, Vertical };

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
        mapper = static_cast<uint8_t>((rom[7] & 0xF0) | (f6 >> 4));
        return true;
    }
};

struct Mapper {
    virtual ~Mapper() = default;
    virtual uint8_t cpu_read(uint16_t addr) = 0;
    virtual void cpu_write(uint16_t addr, uint8_t v) = 0;
    virtual uint8_t ppu_read(uint16_t addr) = 0;
    virtual void ppu_write(uint16_t addr, uint8_t v) = 0;
};

inline uint16_t mirror_translate(uint16_t addr, Mirroring m) {
    addr &= 0x0FFF;
    const uint16_t table = addr >> 10;
    const uint16_t offset = addr & 0x03FF;
    return static_cast<uint16_t>(
        m == Mirroring::Vertical ? (table & 0x01) * 0x400 + offset
                                 : (table >> 1) * 0x400 + offset);
}

/// Mapper-0 cartridge with the CHR-RAM option from the supplied spec:
///
///   header CHR banks == 0  ->  8KB CHR-RAM, fully writable at PPU
///                             $0000-$1FFF (reads AND writes)
///   header CHR banks  > 0  ->  CHR ROM: readable, writes dropped
class ChrRamNROM final : public Mapper {
public:
    std::vector<uint8_t> prg;
    std::vector<uint8_t> chr_rom;         // empty when CHR-RAM
    std::array<uint8_t, 0x2000> chr_ram{};
    bool chr_is_ram = false;
    Mirroring mirroring = Mirroring::Horizontal;
    std::array<uint8_t, 0x800> ciram{};

//@LABS-BEGIN 1
//@LABS-SOLUTION
    uint8_t ppu_read(uint16_t addr) override {
        if (addr < 0x2000)
            return chr_is_ram ? chr_ram[addr & 0x1FFF]
                              : chr_rom[addr % chr_rom.size()];
        return ciram[mirror_translate(addr, mirroring)];
    }

    void ppu_write(uint16_t addr, uint8_t v) override {
        if (addr < 0x2000) {
            if (chr_is_ram) chr_ram[addr & 0x1FFF] = v;
            return;                       // CHR ROM ignores writes
        }
        ciram[mirror_translate(addr, mirroring)] = v;
    }
//@LABS-STUB
    // TODO(1): route PPU traffic per the spec above. Below $2000: CHR-RAM
    // carts read AND write their 8KB window (addresses wrap at $2000);
    // CHR-ROM carts read their ROM and DROP writes. At/above $2000: CIRAM
    // through mirror_translate, both directions.
    uint8_t ppu_read(uint16_t addr) override {
        if (addr < 0x2000) return 0;      // TODO(1)
        return ciram[mirror_translate(addr, mirroring)];
    }

    void ppu_write(uint16_t addr, uint8_t v) override {
        if (addr >= 0x2000)
            ciram[mirror_translate(addr, mirroring)] = v;  // TODO(1)
    }
//@LABS-END

    uint8_t cpu_read(uint16_t addr) override {
        if (addr < 0x8000) return 0;
        return prg[(addr - 0x8000) % prg.size()];
    }
    void cpu_write(uint16_t, uint8_t) override {}

    // Build from a parsed file. chr_is_ram is set purely by the bank count.
    static ChrRamNROM* create(const Header& h,
                              const std::vector<uint8_t>& rom) {
        auto* cart = new ChrRamNROM();
        const size_t skip = 16 + ((rom[6] & 0x04) ? 512 : 0);
        const size_t prg_bytes = size_t(h.prg_banks) * 16384;
        const size_t chr_bytes = size_t(h.chr_banks) * 8192;
        if (rom.size() < skip + prg_bytes + chr_bytes) {
            delete cart;
            return nullptr;
        }
        cart->prg.assign(rom.begin() + long(skip),
                         rom.begin() + long(skip + prg_bytes));
        cart->chr_rom.assign(rom.begin() + long(skip + prg_bytes),
                             rom.begin() + long(skip + prg_bytes + chr_bytes));
        cart->chr_is_ram = h.chr_banks == 0;
        cart->mirroring = h.mirroring;
        return cart;
    }
};

}  // namespace neschr
