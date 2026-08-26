// mbc3.hpp — MBC3 mapper with a deterministic injected-tick RTC.
//
// MBC3 adds two things over MBC1-style banking: a 7-bit ROM bank window
// ($2000-$3FFF) and, on timer carts, a real-time clock exposed through
// the $4000-$5FFF register window. Selecting RAM bank values $08-$0C
// routes $A000-$BFFF reads/writes to five RTC registers instead of SRAM:
//   $08 seconds  $09 minutes  $0A hours  $0B day low 8  $0C day high
// The day-high register carries bit0 = day-count bit 8 and bit6 = HALT.
//
// The small Mapper strategy interface is duplicated here on purpose:
// every exercise must compile standalone.
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace cart {

constexpr uint16_t kRomBankSize = 0x4000;   // 16 KiB
constexpr uint16_t kRamBankSize = 0x2000;   // 8 KiB

class Mapper {  // strategy interface
 public:
    virtual ~Mapper() = default;
    virtual uint8_t readRom(uint16_t addr) const = 0;   // 0000-7FFF
    virtual uint8_t readRam(uint16_t addr) const = 0;   // A000-BFFF
    virtual void writeReg(uint16_t addr, uint8_t val) = 0;
    virtual void writeRam(uint16_t addr, uint8_t val) = 0;
};

constexpr uint64_t kCyclesPerSecond = 4194304;

struct Rtc {
    uint8_t secs = 0;
    uint8_t mins = 0;
    uint8_t hours = 0;
    uint16_t days = 0;    // low 8 bits of the day counter
    uint8_t daysHi = 0;   // bit0: day bit 8, bit6: HALT

    void tick(uint64_t tcycles) {
//@LABS-BEGIN 1
//@LABS-SOLUTION
        if (daysHi & 0x40) return;              // HALT stops everything
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
        // TODO(1): convert whole elapsed seconds out of tcycles and
        // carry them secs->mins->hours->days->day-bit-8. Sub-second
        // remainder is dropped. HALT (daysHi bit 6) must freeze all of it.
        (void)tcycles;
//@LABS-END
    }
};

class Mbc3 final : public Mapper {
 public:
    Mbc3(const uint8_t* rom, size_t romSize, size_t ramSize)
        : rom_(rom), romSize_(romSize), ramSize_(ramSize),
          ram_(ramSize, 0x00) {}

    // ---- test seams -------------------------------------------------
    uint8_t bank1() const { return bank1_; }
    uint8_t ramBankOrRtc() const { return bank2_; }  // 00-03 RAM, 08-0C RTC
    const Rtc& liveRtc() const { return live_; }
    const Rtc& latchedRtc() const { return shadow_; }
    bool frozen() const { return frozen_; }

    void tickRtc(uint64_t tcycles) { live_.tick(tcycles); }

    uint8_t readRom(uint16_t addr) const {
        if (addr < kRomBankSize) return rom_[addr];   // MBC3 has no mode 1
        const size_t off = addr - kRomBankSize;
        return rom_[physicalBankHi() * kRomBankSize + off];
    }

    size_t physicalBankHi() const {
        return static_cast<size_t>(bank1_) % (romSize_ / kRomBankSize);
    }

    uint8_t readRam(uint16_t addr) const {
//@LABS-BEGIN 4
//@LABS-SOLUTION
        if (isRtcSelect(bank2_)) return rtcRegister(bank2_);
        if (!ramEnable_ || ramSize_ == 0) return 0xFF;
        // Bank selects beyond the cart's size mirror low (no OOB).
        const size_t nBanks = std::max<size_t>(ramSize_ / kRamBankSize, 1);
        return ram_[((bank2_ & 0x03u) % nBanks) * kRamBankSize
                   + (addr - kRamBankSize) % ramSize_];
//@LABS-STUB
        // TODO(4): route $A000-$BFFF to the selected RTC register when
        // bank2 holds $08-$0C (shadow values while frozen), otherwise
        // to cart RAM behind the enable gate.
        (void)addr;
        return 0xFF;
//@LABS-END
    }

    void writeReg(uint16_t addr, uint8_t val) {
//@LABS-BEGIN 3
//@LABS-SOLUTION
        if (addr < 0x2000) {
            ramEnable_ = (val & 0x0F) == 0x0A;
        } else if (addr < 0x4000) {
            bank1_ = val & 0x7F;
            if (bank1_ == 0) bank1_ = 1;
        } else if (addr < 0x6000) {
            if (val <= 0x03 || (val >= 0x08 && val <= 0x0C))
                bank2_ = val;                  // anything else: ignored
        } else {
            updateLatch(val);
        }
//@LABS-STUB
        // TODO(3): decode the four windows: RAM enable, 7-bit ROM bank
        // (0 -> 1), RAM-bank-or-RTC-select in $4000-$5FFF, and feed the
        // last window into the latch state machine.
        (void)addr;
        (void)val;
//@LABS-END
    }

    void writeRam(uint16_t addr, uint8_t val) {
//@LABS-BEGIN 5
//@LABS-SOLUTION
        if (isRtcSelect(bank2_)) {
            writeRtcRegister(bank2_, val);     // writes hit LIVE regs only
            return;
        }
        if (!ramEnable_ || ramSize_ == 0) return;
        // Mirror the same way reads do (no OOB on small carts).
        const size_t nBanks = std::max<size_t>(ramSize_ / kRamBankSize, 1);
        ram_[((bank2_ & 0x03u) % nBanks) * kRamBankSize
             + (addr - kRamBankSize) % ramSize_] = val;
//@LABS-STUB
        // TODO(5): writes go to the live RTC register when one is
        // selected, otherwise to enabled cart RAM.
        (void)addr;
        (void)val;
//@LABS-END
    }

 private:
    static bool isRtcSelect(uint8_t v) { return v >= 0x08 && v <= 0x0C; }

    // Latch procedure state machine. Hardware latches on the sequence
    // $00 then $01; any other write restarts the wait for $00.
    void updateLatch(uint8_t val) {
//@LABS-BEGIN 2
//@LABS-SOLUTION
        if ((val & 0x01) == 0x00) {
            latchArmed_ = true;                // waiting for the 01 edge
        } else if (latchArmed_) {
            shadow_ = live_;                   // freeze for reading
            frozen_ = true;
            latchArmed_ = false;
        } else {
            latchArmed_ = false;
        }
//@LABS-STUB
        // TODO(2): implement the 00-then-01 latch handshake. A wrong
        // order (01 first) must NOT latch.
        (void)val;
//@LABS-END
    }

    uint8_t rtcRegister(uint8_t sel) const {
        const Rtc& r = frozen_ ? shadow_ : live_;
        switch (sel) {
            case 0x08: return r.secs;
            case 0x09: return r.mins;
            case 0x0A: return r.hours;
            case 0x0B: return r.days;
            default: return r.daysHi;
        }
    }

    void writeRtcRegister(uint8_t sel, uint8_t val) {
        switch (sel) {
            case 0x08: live_.secs = val; break;
            case 0x09: live_.mins = val; break;
            case 0x0A: live_.hours = val; break;
            case 0x0B: live_.days = val; break;
            case 0x0C: live_.daysHi = val; break;   // incl. HALT bit
            default: break;
        }
    }

    const uint8_t* rom_;
    size_t romSize_;
    size_t ramSize_;
    std::vector<uint8_t> ram_;
    Rtc live_{};
    Rtc shadow_{};
    bool frozen_ = false;       // true: RTC reads come from the shadows
    bool latchArmed_ = false;   // saw $00, waiting for $01
    bool ramEnable_ = false;
    uint8_t bank1_ = 1;
    uint8_t bank2_ = 0;         // RAM bank or RTC select
};

}  // namespace cart
