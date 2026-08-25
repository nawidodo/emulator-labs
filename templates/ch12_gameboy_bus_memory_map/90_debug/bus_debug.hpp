// bus_debug.hpp — bus excerpts under repair in the debugging drill.
//
// THREE defects are seeded below, one per excerpt. Each produces
// plausible-looking traffic, so eyeballing is not enough — isolate each
// one with the failing tests and document it in bug-report.md:
//   bug / root cause / first divergence / fix / regression test.
//
// The excerpts are self-contained on purpose: they mirror structures
// from exercises 01-04 without including them, because real bugs hide
// in code that looks familiar but is not quite your code.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace busdbg {

constexpr uint16_t kEchoBase = 0xE000;
constexpr uint16_t kEchoLen = 0x1E00;  // E000-FDFF mirrors C000-DDFF

// ---- Excerpt A: echo window ------------------------------------------
// Symptom of defect 1: a game that stashes state through the echo
// window appears to lose it — reads alias correctly, so code that only
// READS through E000-FDFF works fine and hides the corruption until
// something reboots.
class DebugEcho {
public:
    explicit DebugEcho(std::vector<uint8_t>& wram) : wram_(wram.data()) {}

    // Read side: verified correct in review — translation is -$2000.
    uint8_t read(uint16_t addr) const {
        return wram_[addr - kEchoBase];
    }

    //@LABS-BEGIN 1
    //@LABS-SOLUTION
    void write(uint16_t addr, uint8_t val) {
        wram_[addr - kEchoBase] = val;
    }
    //@LABS-STUB
    void write(uint16_t addr, uint8_t val) {
        shadow_[addr - kEchoBase] = val;  // TODO(1) BUG: detached buffer swallows the byte
    }
    //@LABS-END

private:
    uint8_t* wram_;
    [[maybe_unused]] uint8_t shadow_[kEchoLen] = {};
};

// ---- Excerpt B: boot-remap handshake ---------------------------------
// Symptom of defect 2: writing FF50 does not always reveal the
// cartridge — sometimes the overlay persists forever, and occasionally
// an unrelated register write knocks it out early.
class DebugBootMapper {
public:
    bool bootMapped = true;

    // Called for EVERY bus write; spec: ANY write to $FF50 unmaps.
    //@LABS-BEGIN 2
    //@LABS-SOLUTION
    void onBusWrite(uint16_t addr, uint8_t val) {
        if (addr == 0xFF50) bootMapped = false;
        (void)val;
    }
    //@LABS-STUB
    void onBusWrite(uint16_t addr, uint8_t val) {
        if (val == 0x50 || addr == 0xFF4F)
            bootMapped = false;  // TODO(2) BUG: compares the wrong operands
        else
            bootMapped = true;
    }
    //@LABS-END
};

// ---- Excerpt C: unusable-page gap ------------------------------------
// Symptom of defect 3: reading FEA0-FEFF after writing there returns
// the last-written byte instead of the documented $00. Games that poll
// the gap as a scratch canary see phantom values.
class DebugGapBus {
public:
    static bool inUnusable(uint16_t addr) {
        return addr >= 0xFEA0 && addr <= 0xFEFF;
    }

    uint8_t read(uint16_t addr) const {
        //@LABS-BEGIN 3
        //@LABS-SOLUTION
        return inUnusable(addr) ? 0x00 : hram_[addr - 0xFF80];
        //@LABS-STUB
        return inUnusable(addr) ? scratch_[addr - 0xFEA0]
                                : hram_[addr - 0xFF80];  // TODO(3) BUG: echoes last write
        //@LABS-END
    }

    void write(uint16_t addr, uint8_t val) {
        if (!inUnusable(addr)) {
            hram_[addr - 0xFF80] = val;
            return;
        }
//@LABS-BEGIN 4
//@LABS-SOLUTION
        // Documented policy: writes into the unusable page are dropped.
//@LABS-STUB
        scratch_[addr - 0xFEA0] = val;  // TODO(4) BUG: scratch RAM absorbs them
//@LABS-END
    }

private:
    [[maybe_unused]] uint8_t scratch_[0x60] = {};  // FEA0-FEFF is exactly 96 bytes
    std::vector<uint8_t> hram_ = std::vector<uint8_t>(0x7F, 0x00);  // FF80-FFFE
};

}  // namespace busdbg
