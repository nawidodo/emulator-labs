// cart.hpp — integrated cartridge front end behind the headless runner:
// header-driven mapper selection (CartridgeController::makeMapper) plus
// ROM_ONLY / MBC1 / MBC3 / MBC5 / MBC-X strategies.
//
// This header duplicates the small per-exercise mapper logic so the
// runner binary stands alone. The @LABS blocks mirror exercises 01-04
// and the unseen-spec coding test; a skeleton build links fine but maps
// nothing until those bodies are filled in.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace cart {

constexpr uint16_t kRomBankSize = 0x4000;
constexpr uint16_t kRamBankSize = 0x2000;
constexpr uint64_t kCyclesPerSecond = 4194304;

class Mapper {  // strategy interface
 public:
    virtual ~Mapper() = default;
    virtual uint8_t readRom(uint16_t addr) const = 0;   // 0000-7FFF
    virtual uint8_t readRam(uint16_t addr) const = 0;   // A000-BFFF
    virtual void writeReg(uint16_t addr, uint8_t val) = 0;
    virtual void writeRam(uint16_t addr, uint8_t val) = 0;
    // ch16 runner extension: advance an injected-tick RTC (MBC3 only).
    virtual void tickRtc(uint64_t /*tcycles*/) {}
};

inline size_t romSizeBytes(uint8_t code) {
//@LABS-BEGIN 1
//@LABS-SOLUTION
    switch (code) {
        case 0x52: return 1048576;
        case 0x53: return 1179648;
        case 0x54: return 1310720;
        default:
            if (code > 0x08) return 0;
            return size_t{32768} << code;
    }
//@LABS-STUB
    // TODO(1): expand the $0148 code into a byte size ($52-$54 are the
    // 1 MiB-class oddballs).
    (void)code;
    return 0;
//@LABS-END
}

inline size_t ramSizeBytes(uint8_t code) {
//@LABS-BEGIN 2
//@LABS-SOLUTION
    switch (code) {
        case 0x01: return 2048;
        case 0x02: return 8192;
        case 0x03: return 32768;
        case 0x04: return 131072;
        case 0x05: return 65536;
        default: return 0;
    }
//@LABS-STUB
    // TODO(2): expand the $0149 code into a byte size.
    (void)code;
    return 0;
//@LABS-END
}

class RomOnly final : public Mapper {
 public:
    RomOnly(const uint8_t* rom, size_t size) : rom_(rom), size_(size) {}
    uint8_t readRom(uint16_t addr) const override {
        return (addr < 0x8000 && addr < size_) ? rom_[addr] : 0xFF;
    }
    // No cart RAM chip on ROM-only boards: open bus both directions.
    uint8_t readRam(uint16_t) const override { return 0xFF; }
    void writeReg(uint16_t, uint8_t) override {}
    void writeRam(uint16_t, uint8_t) override {}

 private:
    const uint8_t* rom_;
    size_t size_;
};

class Mbc1 final : public Mapper {
 public:
    Mbc1(const uint8_t* rom, size_t romSize, size_t ramSize)
        : rom_(rom), nbanks_(romSize / kRomBankSize), ram_(ramSize, 0x00),
          ramSize_(ram_.size()) {}

    uint8_t readRom(uint16_t addr) const override {
        if (nbanks_ == 0) return 0xFF;   // unparseable header: open bus
        const size_t bank = addr < kRomBankSize
            ? (mode_ ? (bank2_ * 32u) % nbanks_ : 0)
            : ((bank2_ * 32u + bank1_) % nbanks_);
        const size_t off =
            (addr < kRomBankSize ? addr : addr - kRomBankSize) +
            bank * kRomBankSize;
        return off < nbanks_ * kRomBankSize ? rom_[off] : 0xFF;
    }

    uint8_t readRam(uint16_t addr) const override {
        if (!ramEnable_ || ramSize_ == 0) return 0xFF;
        const size_t bankOff =
            (mode_ ? bank2_ % (ramSize_ / kRamBankSize) : 0) * kRamBankSize;
        return ram_[bankOff + (addr - kRamBankSize) % ramSize_];
    }

    void writeReg(uint16_t addr, uint8_t val) override {
//@LABS-BEGIN 3
//@LABS-SOLUTION
        if (addr < 0x2000) {
            ramEnable_ = (val & 0x0F) == 0x0A;
        } else if (addr < 0x4000) {
            bank1_ = val & 0x1F;
            if (bank1_ == 0) bank1_ = 1;
        } else if (addr < 0x6000) {
            bank2_ = val & 0x03;
        } else {
            mode_ = (val & 0x01) != 0;
        }
//@LABS-STUB
        // TODO(3): decode the four MBC1 windows exactly as in exercise 02.
        (void)addr;
        (void)val;
//@LABS-END
    }

    void writeRam(uint16_t addr, uint8_t val) override {
        if (!ramEnable_ || ramSize_ == 0) return;
        const size_t bankOff =
            (mode_ ? bank2_ % (ramSize_ / kRamBankSize) : 0) * kRamBankSize;
        ram_[bankOff + (addr - kRamBankSize) % ramSize_] = val;
    }

 private:
    const uint8_t* rom_;
    size_t nbanks_;
    std::vector<uint8_t> ram_;
    size_t ramSize_;
    bool ramEnable_ = false;
    bool mode_ = false;
    uint8_t bank1_ = 1;
    uint8_t bank2_ = 0;
};

struct Rtc {
    uint8_t secs = 0, mins = 0, hours = 0;
    uint16_t days = 0;
    uint8_t daysHi = 0;   // bit0 day 9, bit6 HALT

    void tick(uint64_t tcycles) {
//@LABS-BEGIN 4
//@LABS-SOLUTION
        if (daysHi & 0x40) return;
        uint64_t total = secs + tcycles / kCyclesPerSecond;
        secs = static_cast<uint8_t>(total % 60);
        total = mins + total / 60;
        mins = static_cast<uint8_t>(total % 60);
        total = hours + total / 60;
        hours = static_cast<uint8_t>(total % 24);
        const uint16_t carried = static_cast<uint16_t>(
            ((daysHi & 0x01u) << 8) + days + total / 24);
        days = carried & 0xFF;
        daysHi = static_cast<uint8_t>(
            (daysHi & 0xC0u) | ((carried >> 8) & 0x01u));
//@LABS-STUB
        // TODO(4): carry whole seconds through secs->mins->hours->days,
        // honoring the HALT bit (same model as exercise 03).
        (void)tcycles;
//@LABS-END
    }
};

class Mbc3 final : public Mapper {
 public:
    Mbc3(const uint8_t* rom, size_t romSize, size_t ramSize)
        : rom_(rom), nbanks_(romSize / kRomBankSize), ram_(ramSize, 0x00),
          ramSize_(ram_.size()) {}

    void tickRtc(uint64_t tcycles) override { rtc_.tick(tcycles); }

    uint8_t readRom(uint16_t addr) const override {
        if (nbanks_ == 0) return 0xFF;
        const size_t bank = addr < kRomBankSize
            ? 0
            : static_cast<size_t>(bank1_) % nbanks_;
        const size_t off =
            (addr < kRomBankSize ? addr : addr - kRomBankSize) +
            bank * kRomBankSize;
        return off < nbanks_ * kRomBankSize ? rom_[off] : 0xFF;
    }

    uint8_t readRam(uint16_t addr) const override {
        if (isRtcSel(bank2_)) return rtcRegister(bank2_);
        if (!ramEnable_ || ramSize_ == 0) return 0xFF;
        const size_t bankOff =
            static_cast<size_t>(bank2_ % (ramSize_ / kRamBankSize));
        return ram_[bankOff * kRamBankSize +
                    (addr - kRamBankSize) % ramSize_];
    }

    void writeReg(uint16_t addr, uint8_t val) override {
//@LABS-BEGIN 5
//@LABS-SOLUTION
        if (addr < 0x2000) {
            ramEnable_ = (val & 0x0F) == 0x0A;
        } else if (addr < 0x4000) {
            bank1_ = val & 0x7F;
            if (bank1_ == 0) bank1_ = 1;
        } else if (addr < 0x6000) {
            if (val <= 0x03 || (val >= 0x08 && val <= 0x0C))
                bank2_ = val;
        } else if ((val & 0x01) == 0x00) {
            latchArmed_ = true;      // waiting for the 01 edge
        } else if (latchArmed_) {
            shadow_ = rtc_;          // 00-then-01 handshake completes
            frozen_ = true;
            latchArmed_ = false;
        } else {
            latchArmed_ = false;
        }
//@LABS-STUB
        // TODO(5): decode the four MBC3 windows including the 00-then-01
        // RTC latch handshake (same model as exercise 03).
        (void)addr;
        (void)val;
//@LABS-END
    }

    void writeRam(uint16_t addr, uint8_t val) override {
        if (isRtcSel(bank2_)) {
            switch (bank2_) {
                case 0x08: rtc_.secs = val; break;
                case 0x09: rtc_.mins = val; break;
                case 0x0A: rtc_.hours = val; break;
                case 0x0B: rtc_.days = val; break;
                case 0x0C: rtc_.daysHi = val; break;
                default: break;
            }
            return;
        }
        if (!ramEnable_ || ramSize_ == 0) return;
        const size_t bankOff =
            static_cast<size_t>(bank2_ % (ramSize_ / kRamBankSize));
        ram_[bankOff * kRamBankSize + (addr - kRamBankSize) % ramSize_] =
            val;
    }

 private:
    static bool isRtcSel(uint8_t v) { return v >= 0x08 && v <= 0x0C; }

    uint8_t rtcRegister(uint8_t sel) const {
        const Rtc& r = frozen_ ? shadow_ : rtc_;
        switch (sel) {
            case 0x08: return r.secs;
            case 0x09: return r.mins;
            case 0x0A: return r.hours;
            case 0x0B: return r.days;
            default: return r.daysHi;
        }
    }

    const uint8_t* rom_;
    size_t nbanks_;
    std::vector<uint8_t> ram_;
    size_t ramSize_;
    Rtc rtc_{}, shadow_{};
    bool frozen_ = false, latchArmed_ = false;
    bool ramEnable_ = false;
    uint8_t bank1_ = 1, bank2_ = 0;
};

class Mbc5 final : public Mapper {
 public:
    Mbc5(const uint8_t* rom, size_t romSize, size_t ramSize)
        : rom_(rom), nbanks_(romSize / kRomBankSize), ram_(ramSize, 0x00),
          ramSize_(ram_.size()) {}

    uint8_t readRom(uint16_t addr) const override {
        if (nbanks_ == 0) return 0xFF;
        const size_t bank = addr < 0x4000
            ? 0
            : static_cast<size_t>(romBank_) % nbanks_;
        const size_t off =
            (addr < 0x4000 ? addr : addr - kRomBankSize) +
            bank * kRomBankSize;
        return off < nbanks_ * kRomBankSize ? rom_[off] : 0xFF;
    }

    uint8_t readRam(uint16_t addr) const override {
        if (!ramEnable_ || ramSize_ == 0) return 0xFF;
        const size_t bankOff =
            static_cast<size_t>(ramBank_ % (ramSize_ / kRamBankSize));
        return ram_[bankOff * kRamBankSize +
                    (addr - kRamBankSize) % ramSize_];
    }

    void writeReg(uint16_t addr, uint8_t val) override {
//@LABS-BEGIN 6
//@LABS-SOLUTION
        if (addr < 0x2000) {
            ramEnable_ = (val & 0x0F) == 0x0A;
        } else if (addr < 0x3000) {
            romBank_ = static_cast<uint16_t>((romBank_ & 0x0100u) | val);
        } else if (addr < 0x4000) {
            if (val & 0x01)
                romBank_ |= 0x0100;
            else
                romBank_ &= 0x00FF;
        } else if (addr < 0x6000) {
            ramBank_ = val & 0x0F;
        }
//@LABS-STUB
        // TODO(6): decode the MBC5 windows including the 9th ROM bank
        // bit at $3000-$3FFF (same model as exercise 04).
        (void)addr;
        (void)val;
//@LABS-END
    }

    void writeRam(uint16_t addr, uint8_t val) override {
        if (!ramEnable_ || ramSize_ == 0) return;
        const size_t bankOff =
            static_cast<size_t>(ramBank_ % (ramSize_ / kRamBankSize));
        ram_[bankOff * kRamBankSize + (addr - kRamBankSize) % ramSize_] =
            val;
    }

 private:
    const uint8_t* rom_;
    size_t nbanks_;
    std::vector<uint8_t> ram_;
    size_t ramSize_;
    bool ramEnable_ = false;
    uint16_t romBank_ = 0;
    uint8_t ramBank_ = 0;
};

// MBC-X: the simplified unseen mapper specified in 99_coding_test/
// CODING_TEST.md (type $BE, 3-bit ROM bank select, soft open-bus switch).
class MbcX final : public Mapper {
 public:
    MbcX(const uint8_t* rom, size_t size) : rom_(rom), size_(size) {}

    uint8_t readRom(uint16_t addr) const override {
        if (addr < 0x4000) return addr < size_ ? rom_[addr] : 0xFF;
        if (openBus_) return 0xFF;
        const size_t off = (addr - 0x4000) + bank1_ * 0x4000;
        return off < size_ ? rom_[off] : 0xFF;
    }
    // Spec: no RAM at all.
    uint8_t readRam(uint16_t) const override { return 0xFF; }

    void writeReg(uint16_t addr, uint8_t val) override {
//@LABS-BEGIN 7
//@LABS-SOLUTION
        if (addr >= 0x2000 && addr < 0x3000) {
            bank1_ = val & 0x07;
            if (bank1_ == 0) bank1_ = 1;
        } else if (addr >= 0x4000 && addr < 0x5000) {
            openBus_ = (val & 0x01) != 0;
        }
//@LABS-STUB
        // TODO(7): implement the MBC-X register writes per CODING_TEST.md
        // ($2000-$2FFF 3-bit bank, $4000-$4FFF open-bus switch).
        (void)addr;
        (void)val;
//@LABS-END
    }
    void writeRam(uint16_t, uint8_t) override {}

 private:
    const uint8_t* rom_;
    size_t size_;
    uint8_t bank1_ = 1;
    bool openBus_ = false;
};

struct CartridgeController {
    // Header-driven strategy selection over the full ch16 mapper set.
    static std::unique_ptr<Mapper> makeMapper(const uint8_t* rom,
                                              size_t size) {
        if (size < 0x150) return nullptr;
        const uint8_t type = rom[0x147];
        const size_t ramSize = ramSizeBytes(rom[0x149]);
        if (type >= 0x01 && type <= 0x03)
            return std::make_unique<Mbc1>(rom, size, ramSize);
        if (type == 0x0F || type == 0x10 || type == 0x12 || type == 0x13)
            return std::make_unique<Mbc3>(rom, size, ramSize);
        if (type >= 0x19 && type <= 0x1E)
            return std::make_unique<Mbc5>(rom, size, ramSize);
        if (type == 0xBE) return std::make_unique<MbcX>(rom, size);
        return std::make_unique<RomOnly>(rom, size);  // incl. $00/unknown
    }
};

}  // namespace cart
