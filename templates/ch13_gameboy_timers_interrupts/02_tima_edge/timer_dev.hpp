#pragma once
#include <cstdint>

#include "../01_divider/divider.hpp"

namespace gb {

// Exercise 13.02 -- wiring the timer unit onto the divider: the TAC gate
// ($FF07 bit 2) and the falling-edge detector that clocks TIMA ($FF05).
//
// Exact bit-select contract (memorize it, the tests do):
//
//   TAC bits 1-0   tapped DIV counter bit   TIMA tick period
//   ------------   ---------------------    ----------------
//        00                  9                1024 T-cycles
//        01                  3                  16 T-cycles
//        10                  5                  64 T-cycles
//        11                  7                 256 T-cycles
//
// Model contract (deterministic by construction):
//   * The selected bit is SAMPLED once per 4-T-cycle block, gated or not;
//     sampling is a pure observer and never stops.
//   * While TAC.2 is set, a sampled 1->0 transition increments TIMA.
//     Enabling TAC "mid-tick" therefore behaves sanely: if the tapped bit
//     already reads 1, its next natural fall still counts.
//   * Disabling TAC (bit 2 1->0) while the tapped bit reads 0 AFTER the
//     write produces exactly one increment -- the disable itself is the
//     edge on real hardware. This is the documented model contract; see
//     LECTURE.md ("the disable edge").
//   * A select change while running can make the sample stream appear to
//     fall (old bit 1 -> new bit 0); that counts as an edge too, matching
//     hardware's mux glitch behavior.

struct TimerDevice {
    Divider div;
    uint8_t tima = 0x00;             // $FF05
    uint8_t tma = 0x00;              // $FF06 reload value
    uint8_t tac = 0x00;              // $FF07 low bits only; reads OR $F8
    bool overflow_pulse = false;     // set when TIMA wraps 0xFF->0x00 this step
    bool prev_sample = false;        // last sampled value of the tapped bit

    //@LABS-BEGIN 1
    //@LABS-SOLUTION
    // Which DIV counter bit feeds TIMA for the current TAC select value:
    // 00->bit 9, 01->bit 3, 10->bit 5, 11->bit 7. These are the four
    // familiar timer rates 4096/262144/65536/16384 Hz.
    static int select_shift(uint8_t tac_value) {
        switch (tac_value & 0x03) {
            case 0: return 9;
            case 1: return 3;
            case 2: return 5;
            default: return 7;
        }
    }
    //@LABS-STUB
    // TODO(1): map TAC bits 1-0 to the tapped DIV bit: 00->9, 01->3,
    // 10->5, 11->7.
    static int select_shift(uint8_t tac_value) {
        (void)tac_value;
        return -1;  // wrong on purpose
    }
    //@LABS-END

    // Current value of the tapped bit inside the internal divider counter.
    [[nodiscard]] bool selected_bit() const {
        return ((div.counter >> select_shift(tac)) & 1u) != 0;
    }

    [[nodiscard]] bool enabled() const { return (tac & 0x04) != 0; }

    // $FF07 reads return 111TT where TT is bits 1-0 (upper bits read 1).
    [[nodiscard]] uint8_t read_tac() const {
        return static_cast<uint8_t>(tac | 0xF8);
    }

    // TIMA increment with the overflow strobe. Deliberately dumb: it wraps
    // to $00 and pulses; WHO reloads TMA and raises IF is exercise 03's
    // policy layer (real hardware takes 4 extra T-cycles to do both).
    void increment_tima() {
        if (++tima == 0x00) overflow_pulse = true;
    }

    //@LABS-BEGIN 2
    //@LABS-SOLUTION
    // Advance `cycles` (whole 4-T-cycle blocks). Per block: move the
    // divider, sample the tapped bit, and count one TIMA tick on a gated
    // 1->0 transition. Sampling continues while disabled so that re-enabling
    // mid-bit behaves deterministically.
    void step(int cycles) {
        for (int blocks = cycles / 4; blocks > 0; --blocks) {
            div.step(4);
            const bool cur = selected_bit();
            if (enabled() && prev_sample && !cur) increment_tima();
            prev_sample = cur;
        }
    }
    //@LABS-STUB
    // TODO(2): per 4-cycle block advance the divider, re-sample the tapped
    // bit, and increment TIMA on a gated (TAC.2 set) 1->0 transition.
    void step(int cycles) {
        (void)cycles;
    }
    //@LABS-END

    //@LABS-BEGIN 3
    //@LABS-SOLUTION
    // TAC register write ($FF07). Only bits 2-0 are meaningful. Besides
    // latching the value, a DISABLE (bit 2 1->0) whose tapped bit reads 0
    // after the write generates exactly one TIMA increment: on silicon the
    // AND-gate output falls right here, and a fall is a fall.
    void write_tac(uint8_t value) {
        const bool was_enabled = enabled();
        tac = static_cast<uint8_t>(value & 0x07);
        if (was_enabled && !enabled() && !selected_bit()) increment_tima();
    }
    //@LABS-STUB
    // TODO(3): latch TAC bits 2-0; when the write DISABLES the timer
    // (bit 2 was 1, now 0) and the tapped bit reads 0 afterwards, produce
    // exactly one TIMA increment (the disable edge).
    void write_tac(uint8_t value) {
        tac = static_cast<uint8_t>(value & 0x07);
    }
    //@LABS-END
};

}  // namespace gb
