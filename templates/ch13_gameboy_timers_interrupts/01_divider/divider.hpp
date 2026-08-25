#pragma once
#include <cstdint>

namespace gb {

// Exercise 13.01 -- the SM83 divider. The real chip has ONE free-running
// 16-bit counter behind $FF04; DIV is nothing but its HIGH byte, and the
// timer unit of exercise 02 taps individual bits of the same counter.
// That is why writing DIV disturbs TIMA on hardware: resetting the counter
// yanks every tapped bit to 0 at once.

struct Divider {
    uint16_t counter = 0;  // internal; the visible register is only bits 15-8

    //@LABS-BEGIN 1
    //@LABS-SOLUTION
    // Advance the internal counter. The machine drives everything in whole
    // T-cycle blocks of 4 (one memory access per M-cycle), so `cycles` is
    // consumed in 4-cycle steps (4 counts each); a remainder smaller than
    // 4 is dropped -- no SM83 instruction ever produces one. The counter
    // therefore gains one count per T-cycle.
    void step(int cycles) {
        for (int blocks = cycles / 4; blocks > 0; --blocks)
            counter = static_cast<uint16_t>(counter + 4);
    }
    //@LABS-STUB
    // TODO(1): advance the counter by whole 4-T-cycle blocks (cycles/4 of
    // them, 4 counts each). Drop any sub-block remainder.
    void step(int cycles) {
        (void)cycles;
    }
    //@LABS-END

    //@LABS-BEGIN 2
    //@LABS-SOLUTION
    // DIV register read: the high byte of the internal counter, so the
    // visible register steps once every 256 T-cycles (16384 Hz).
    [[nodiscard]] uint8_t div() const {
        return static_cast<uint8_t>(counter >> 8);
    }
    //@LABS-STUB
    // TODO(2): return the HIGH byte of the internal counter ($FF04 reads).
    [[nodiscard]] uint8_t div() const {
        return 0xFF;  // wrong on purpose
    }
    //@LABS-END

    //@LABS-BEGIN 3
    //@LABS-SOLUTION
    // Any write to $FF04 resets the WHOLE 16-bit counter, not just the
    // visible byte. Hardware quirk this models: if the timer unit is
    // tapping a bit that read 1 before the write, the reset IS a falling
    // edge and TIMA ticks (exercise 02 observes that through its sampler).
    void write_div() {
        counter = 0;
    }
    //@LABS-STUB
    // TODO(3): any write to DIV clears the entire internal counter.
    void write_div() {}
    //@LABS-END
};

}  // namespace gb
