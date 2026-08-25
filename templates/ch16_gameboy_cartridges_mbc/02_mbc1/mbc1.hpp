// mbc1.hpp — MBC1 mapper (strategy class) and the CartridgeController
// factory entry point for this exercise.
//
// MBC1 in one paragraph: writes to $0000-$7FFF never reach ROM; they
// land in four register windows. $0000-$1FFF enables cart RAM when the
// low nibble is $A. $2000-$3FFF sets bank1 = low 5 bits of the value,
// with the hardware quirk that 0 becomes 1. $4000-$5FFF sets bank2 =
// low 2 bits. $6000-$7FFF latches the banking MODE from bit 0:
//   mode 0: low ROM half always shows physical bank 0, bank2 supplies
//           ROM address bits 19-20 for the high half.
//   mode 1: bank2 also selects the physical bank shown at $0000-$3FFF
//           and selects one of up to four 8 KiB RAM banks.
// All banks wrap modulo the cartridge size — out-of-range selects are
// masked by hardware, never open bus.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace cart {

constexpr uint16_t kRomBankSize = 0x4000;   // 16 KiB
constexpr uint16_t kRamBankSize = 0x2000;   // 8 KiB

class Mapper {  // strategy interface (duplicated per exercise on purpose)
 public:
    virtual ~Mapper() = default;
    virtual uint8_t readRom(uint16_t addr) const = 0;   // 0000-7FFF
    virtual uint8_t readRam(uint16_t addr) const = 0;   // A000-BFFF
    virtual void writeReg(uint16_t addr, uint8_t val) = 0;
    virtual void writeRam(uint16_t addr, uint8_t val) = 0;
};

class Mbc1 final : public Mapper {
 public:
    Mbc1(const uint8_t* rom, size_t romSize, size_t ramSize)
        : rom_(rom), romSize_(romSize), ramSize_(ramSize),
          ram_(ramSize, 0x00) {}

    // ---- test seams -------------------------------------------------
    bool ramEnabled() const { return ramEnable_; }
    uint8_t bank1() const { return bank1_; }
    uint8_t bank2() const { return bank2_; }
    bool modeOne() const { return mode_; }
    // Physical bank mapped at $4000-$7FFF right now.
    size_t physicalBankHi() const {
        return static_cast<size_t>(((bank2_ << 5) | bank1_)) %
               (romSize_ / kRomBankSize);
    }

    uint8_t readRom(uint16_t addr) const {
        if (addr < kRomBankSize) {
            const size_t bank = mode_
                ? (static_cast<size_t>(bank2_) << 5) % nBanks()
                : 0;
            return rom_[bank * kRomBankSize + addr];
        }
        const size_t off = addr - kRomBankSize;
        return rom_[physicalBankHi() * kRomBankSize + off];
    }

    uint8_t readRam(uint16_t addr) const {
//@LABS-BEGIN 3
//@LABS-SOLUTION
        if (!ramEnable_ || ramSize_ == 0) return 0xFF;
        const size_t bank =
            mode_ ? (bank2_ & 0x03u) : 0;   // mode 0 pins RAM bank 0
        const size_t off = (addr - kRamBankSize) % ramSize_;
        return ram_[bank * kRamBankSize + off];
//@LABS-STUB
        // TODO(3): gate on RAM enable, pick the mode-1 bank, mask the
        // offset by the cart's RAM size. Disabled/absent RAM reads $FF.
        (void)addr;
        return 0xFF;
//@LABS-END
    }

    void writeReg(uint16_t addr, uint8_t val) {
//@LABS-BEGIN 1
//@LABS-SOLUTION
        if (addr < 0x2000) {
            ramEnable_ = (val & 0x0F) == 0x0A;
        } else if (addr < 0x4000) {
            bank1_ = val & 0x1F;
            if (bank1_ == 0) bank1_ = 1;   // hardware: bank 0 wraps to 1
        } else if (addr < 0x6000) {
            bank2_ = val & 0x03;
        } else {
            mode_ = (val & 0x01) != 0;
        }
//@LABS-STUB
        // TODO(1): decode the four MBC1 register windows ($0000-$1FFF
        // RAM enable, $2000-$3FFF bank1 low-5-bits/0->1, $4000-$5FFF
        // bank2 low 2 bits, $6000-$7FFF mode select).
        (void)addr;
        (void)val;
//@LABS-END
    }

    void writeRam(uint16_t addr, uint8_t val) {
//@LABS-BEGIN 4
//@LABS-SOLUTION
        if (!ramEnable_ || ramSize_ == 0) return;   // writes fall silent
        const size_t bank = mode_ ? (bank2_ & 0x03u) : 0;
        const size_t off = (addr - kRamBankSize) % ramSize_;
        ram_[bank * kRamBankSize + off] = val;
//@LABS-STUB
        // TODO(4): store into cart RAM only while enabled; mirror the
        // read-side bank/mask math.
        (void)addr;
        (void)val;
//@LABS-END
    }

 private:
    size_t nBanks() const { return romSize_ / kRomBankSize; }

    const uint8_t* rom_;
    size_t romSize_;
    size_t ramSize_;
    std::vector<uint8_t> ram_;
    bool ramEnable_ = false;
    bool mode_ = false;
    uint8_t bank1_ = 1;   // power-up: bank 1 visible at $4000
    uint8_t bank2_ = 0;
};

// Factory hook for this exercise: anything MBC1-family ($01-$03) gets an
// Mbc1; other types are not this exercise's business and yield nullptr.
struct CartridgeController {
    static std::unique_ptr<Mapper> makeMapper(const uint8_t* rom,
                                              size_t size);
};

inline std::unique_ptr<Mapper> CartridgeController::makeMapper(
    const uint8_t* rom, size_t size) {
    if (size < 0x150) return nullptr;
    const uint8_t type = rom[0x147];
    if (type >= 0x01 && type <= 0x03)
        return std::make_unique<Mbc1>(rom, size, 0x2000);  // 8 KiB lab RAM
    return nullptr;
}

}  // namespace cart
