// mbc5.hpp — MBC5 mapper with battery-backed SRAM save/load.
//
// MBC5 dropped MBC1's mode register and its bank-0-quirk and finally
// gave games a full 9-bit ROM bank number:
//   $2000-$2FFF  low 8 bits of the ROM bank
//   $3000-$3FFF  sets bit 9 of the ROM bank
//   $4000-$5FFF  RAM bank 0-15 (no mode quirk, no wrap-to-1)
// A 9-bit bank selects any of up to 512 banks = 8 MiB, the largest GB
// carts ever shipped. Bank 0 IS selectable here ($2000 write of 0 keeps
// bank 0 visible at $4000-$7FFF), another break with MBC1.
//
// Battery SRAM survives power loss; the emulator persists the RAM array
// verbatim as an exact-size file (ramSizeBytes bytes, no header).
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

namespace cart {

class Mapper {  // strategy interface (duplicated per exercise on purpose)
 public:
    virtual ~Mapper() = default;
    virtual uint8_t readRom(uint16_t addr) const = 0;   // 0000-7FFF
    virtual uint8_t readRam(uint16_t addr) const = 0;   // A000-BFFF
    virtual void writeReg(uint16_t addr, uint8_t val) = 0;
    virtual void writeRam(uint16_t addr, uint8_t val) = 0;
};

class Mbc5 final : public Mapper {
 public:
    Mbc5(const uint8_t* rom, size_t romSize, size_t ramSize)
        : rom_(rom), romSize_(romSize), ramSize_(ramSize),
          ram_(ramSize, 0x00) {}

    // ---- test seams -------------------------------------------------
    bool ramEnabled() const { return ramEnable_; }
    uint16_t romBank9() const { return romBank_; }
    uint8_t ramBank() const { return ramBank_; }
    // Physical bank mapped at $4000-$7FFF right now (the 9-bit select
    // wrapped by the cartridge size).
    size_t computedBank(uint16_t /*addr*/) const {
        return static_cast<size_t>(romBank_) % nBanks();
    }

    uint8_t readRom(uint16_t addr) const {
        if (addr < 0x4000) return rom_[addr];
        const size_t off = addr - 0x4000;
        return rom_[computedBank(addr) * 0x4000 + off];
    }

    void writeReg(uint16_t addr, uint8_t val) {
//@LABS-BEGIN 1
//@LABS-SOLUTION
        if (addr < 0x2000) {
            ramEnable_ = (val & 0x0F) == 0x0A;
        } else if (addr < 0x3000) {
            romBank_ = static_cast<uint16_t>((romBank_ & 0x0100u) | val);
        } else if (addr < 0x4000) {
            if (val & 0x01)
                romBank_ = static_cast<uint16_t>(romBank_ | 0x0100u);
            else
                romBank_ = static_cast<uint16_t>(romBank_ & 0x00FFu);
        } else if (addr < 0x6000) {
            ramBank_ = val & 0x0F;
        }
//@LABS-STUB
        // TODO(1): decode the windows: RAM enable nibble, low 8 ROM
        // bank bits ($2000-$2FFF), ROM bank bit 9 ($3000-$3FFF), RAM
        // bank 0-15 ($4000-$5FFF).
        (void)addr;
        (void)val;
//@LABS-END
    }

    uint8_t readRam(uint16_t addr) const {
//@LABS-BEGIN 3
//@LABS-SOLUTION
        if (!ramEnable_ || ramSize_ == 0) return 0xFF;
        const size_t off =
            (addr - 0xA000) % ramSize_;
        const size_t bankOff =
            (static_cast<size_t>(ramBank_) % (ramSize_ / 0x2000)) * 0x2000;
        return ram_[bankOff + off];
//@LABS-STUB
        // TODO(3): gate on enable, route through the selected RAM bank.
        (void)addr;
        return 0xFF;
//@LABS-END
    }

    void writeRam(uint16_t addr, uint8_t val) {
//@LABS-BEGIN 4
//@LABS-SOLUTION
        if (!ramEnable_ || ramSize_ == 0) return;
        const size_t off = (addr - 0xA000) % ramSize_;
        const size_t bankOff =
            (static_cast<size_t>(ramBank_) % (ramSize_ / 0x2000)) * 0x2000;
        ram_[bankOff + off] = val;
//@LABS-STUB
        // TODO(4): mirror the read-side banking math for writes.
        (void)addr;
        (void)val;
//@LABS-END
    }

    // Battery persistence: exact-size raw dump of cart SRAM.
    bool saveSram(const char* path) const {
//@LABS-BEGIN 5
//@LABS-SOLUTION
        FILE* f = std::fopen(path, "wb");
        if (!f) return false;
        const size_t n = std::fwrite(ram_.data(), 1, ram_.size(), f);
        return std::fclose(f) == 0 && n == ram_.size();
//@LABS-STUB
        // TODO(5): write the whole SRAM array to PATH as an exact-size
        // raw file.
        (void)path;
        return false;
//@LABS-END
    }

    bool loadSram(const char* path) {
//@LABS-BEGIN 6
//@LABS-SOLUTION
        FILE* f = std::fopen(path, "rb");
        if (!f) return false;
        std::vector<uint8_t> staged(ram_.size());
        const size_t n = std::fread(staged.data(), 1, staged.size(), f);
        std::fclose(f);
        if (n != staged.size()) return false;   // short file: keep old SRAM
        ram_ = staged;
        return true;
//@LABS-STUB
        // TODO(6): read exactly ramSize() bytes into SRAM; fail without
        // corrupting state when the file is missing or truncated.
        (void)path;
        return false;
//@LABS-END
    }

    size_t ramSize() const { return ramSize_; }

 private:
    size_t nBanks() const { return romSize_ / 0x4000; }

    const uint8_t* rom_;
    size_t romSize_;
    size_t ramSize_;
    std::vector<uint8_t> ram_;
    bool ramEnable_ = false;
    uint16_t romBank_ = 0;   // MBC5 powers up showing bank 0 at $4000
    uint8_t ramBank_ = 0;
};

struct CartridgeController {
    static std::unique_ptr<Mapper> makeMapper(const uint8_t* rom,
                                              size_t size);
};

inline std::unique_ptr<Mapper> CartridgeController::makeMapper(
    const uint8_t* rom, size_t size) {
    if (size < 0x150) return nullptr;
    const uint8_t type = rom[0x147];
    if (type >= 0x19 && type <= 0x1E)
        return std::make_unique<Mbc5>(rom, size, 0x2000);  // 8 KiB lab RAM
    return nullptr;
}

}  // namespace cart
