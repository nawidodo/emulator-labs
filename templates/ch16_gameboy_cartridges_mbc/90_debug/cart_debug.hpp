// cart_debug.hpp — mapper excerpts under repair in the debugging drill.
//
// THREE defects are seeded below, one per real mapper. Each produces
// plausible-looking bus traffic, so eyeballing is not enough — isolate
// each one with the failing tests and document it in bug-report.md:
//   bug / root cause / first divergence / fix / regression test.
#pragma once

#include <cstddef>
#include <cstdint>

namespace cartdbg {

constexpr uint16_t kRomBankSize = 0x4000;

// ---- MBC1 excerpt ----------------------------------------------------
// Symptom of defect 1: selecting banks above 31 lands on wrong physical
// banks (e.g. $21 selects bank 33 instead of bank 1) on any cart larger
// than 512 KiB... and even small carts mis-map when games write values
// with upper garbage bits set.
struct DebugMbc1 {
    const uint8_t* rom;
    size_t nbanks;      // physical 16 KiB banks in the image
    uint8_t bank1 = 1;
    uint8_t bank2 = 0;

    void writeBank1(uint8_t val) {
//@LABS-BEGIN 1
//@LABS-STUB
        // BUG(1) + TODO(1): masks SIX bits where MBC1's bank1 register is five.
        bank1 = val & 0x3F;
//@LABS-SOLUTION
        bank1 = val & 0x1F;
//@LABS-END
    }

    // Physical bank visible at $4000-$7FFF.
    size_t physicalBankHi() const {
        return static_cast<size_t>((bank2 << 5) | bank1) % nbanks;
    }
};

// ---- MBC3 excerpt ----------------------------------------------------
// Symptom of defect 2: RTC reads never freeze when a game follows the
// documented 00-then-01 handshake, but DO freeze on the inverted order —
// timer-dependent code appears to run on a stopped clock (or vice versa).
struct DebugMbc3 {
    uint8_t liveSecs = 0;
    uint8_t shadowSecs = 0;
    bool frozen = false;
    bool armed = false;   // saw the first half of the handshake

    void latchClock(uint8_t val) {
//@LABS-BEGIN 2
//@LABS-STUB
        // BUG(2) + TODO(2): completes the handshake on 01-then-00, the inverse of
        // the hardware order.
        // BUG(2): completes on the INVERTED order (01 then 00).
        if (!armed && val == 0x01) {
            armed = true;
        } else if (armed && val == 0x00) {
            shadowSecs = liveSecs;
            frozen = true;
            armed = false;
        } else {
            armed = false;
        }
//@LABS-SOLUTION
        if (val == 0x00) {
            armed = true;              // waiting for the 01 edge
        } else if (armed && val == 0x01) {
            shadowSecs = liveSecs;     // freeze for reading
            frozen = true;
            armed = false;
        } else {
            armed = false;
        }
//@LABS-END
    }

    uint8_t readSeconds() const { return frozen ? shadowSecs : liveSecs; }
};

// ---- MBC5 excerpt ----------------------------------------------------
// Symptom of defect 3: 8 MiB-class games (bank selects >= 256) wrap back
// into the first 4 MiB — later levels load earlier levels' graphics.
struct DebugMbc5 {
    size_t nbanks;      // physical banks; >256 exercises the high bit
    uint16_t romBank = 0;

    void writeRegHigh(uint8_t val) {
//@LABS-BEGIN 3
//@LABS-STUB
        // BUG(3) + TODO(3): ignores bit 9 entirely — writes here are dropped.
        (void)val;
//@LABS-SOLUTION
        if (val & 0x01)
            romBank |= 0x0100;
        else
            romBank &= 0x00FF;
//@LABS-END
    }

    void writeRegLow(uint8_t val) {
        // The low window keeps bit 9 intact (not under repair).
        romBank = static_cast<uint16_t>((romBank & 0x0100u) | val);
    }

    // Physical bank visible at $4000-$7FFF.
    size_t computedBank() const {
        return static_cast<size_t>(romBank) % nbanks;
    }
};

}  // namespace cartdbg
