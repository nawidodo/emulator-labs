// timer_debug.hpp — timer excerpts under repair in the debugging drill.
//
// THREE defects are seeded below, one per excerpt. Each produces
// plausible-looking timer behavior — a clock that ticks, an interrupt
// that fires — so eyeballing is not enough; isolate each one with its
// failing tests and document it in bug-report.md:
//   bug / root cause / first divergence / fix / regression test.
//
// These are SELF-CONTAINED excerpts: each struct re-implements just enough
// divider/edge machinery to expose exactly one defect.
#pragma once

#include <cstdint>

namespace gbdbg {

//@LABS-BEGIN 1
//@LABS-SOLUTION
// ---- excerpt A: TAC gate --------------------------------------------
// Correct version: TAC bit 2 ($FF07) is the timer enable. Bits 1-0 only
// pick which DIV counter bit feeds TIMA.
struct GateTimer {
    uint16_t cnt = 0;
    uint8_t tima = 0;
    uint8_t tac = 0;

    void step(int cycles) {
        const bool en = (tac & 0x04) != 0;
        for (int blocks = cycles / 4; blocks > 0; --blocks) {
            cnt = static_cast<uint16_t>(cnt + 4);
            const int shift = (tac & 0x03) == 0 ? 9 : (tac & 0x03) == 1 ? 3
                              : (tac & 0x03) == 2 ? 5 : 7;
            const bool cur = ((cnt >> shift) & 1u) != 0;
            if (en && prev_bit && !cur) ++tima;
            prev_bit = cur;
        }
    }

    bool prev_bit = false;
};
//@LABS-STUB
// TODO(1): find and fix the seeded defect in this gate logic.
// Symptom of defect 1: the enable gate tests the WRONG TAC BIT, so with
// select=00 ($04, the classic 4096 Hz setup) the timer looks dead while
// select=01 with bit 2 CLEAR ($01) still ticks -- games see either a
// frozen TIMA or a timer that ignores being switched off.
struct GateTimer {
    uint16_t cnt = 0;
    uint8_t tima = 0;
    uint8_t tac = 0;

    void step(int cycles) {  // TODO(1)
        const bool en = (tac & 0x01) != 0;  // seeded bug: wrong gate bit
        for (int blocks = cycles / 4; blocks > 0; --blocks) {
            cnt = static_cast<uint16_t>(cnt + 4);
            const int shift = (tac & 0x03) == 0 ? 9 : (tac & 0x03) == 1 ? 3
                              : (tac & 0x03) == 2 ? 5 : 7;
            const bool cur = ((cnt >> shift) & 1u) != 0;
            if (en && prev_bit && !cur) ++tima;
            prev_bit = cur;
        }
    }

    bool prev_bit = false;
};
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// ---- excerpt B: the disable edge ------------------------------------
// Correct version: DISABLING the timer (TAC bit 2 1->0) while the tapped
// DIV bit reads 0 AFTER the write generates exactly one TIMA increment --
// on silicon the AND-gate output falls at the write itself.
struct DisableTimer {
    uint16_t cnt = 0;
    uint8_t tima = 0;
    uint8_t tac = 0;

    static int shift_of(uint8_t t) {
        return (t & 0x03) == 0 ? 9 : (t & 0x03) == 1 ? 3
               : (t & 0x03) == 2 ? 5 : 7;
    }

    [[nodiscard]] bool selected_bit() const {
        return ((cnt >> shift_of(tac)) & 1u) != 0;
    }

    void step(int cycles) {
        const bool en = (tac & 0x04) != 0;
        for (int blocks = cycles / 4; blocks > 0; --blocks) {
            cnt = static_cast<uint16_t>(cnt + 4);
            const bool cur = selected_bit();
            if (en && prev_bit && !cur) ++tima;
            prev_bit = cur;
        }
    }

    // Write $FF07: latch bits 2-0; a disable with tapped bit 0 afterwards
    // is itself a falling edge -> one increment.
    void write_tac(uint8_t value) {
        const bool was_enabled = (tac & 0x04) != 0;
        tac = static_cast<uint8_t>(value & 0x07);
        if (was_enabled && !enabled() && !selected_bit()) ++tima;
    }

    [[nodiscard]] bool enabled() const { return (tac & 0x04) != 0; }
    bool prev_bit = false;
};
//@LABS-STUB
// TODO(2): find and fix the seeded defect in this TAC write path.
// Symptom of defect 2: switching the timer OFF loses one tick. Disabling
// TAC while the tapped DIV bit reads 0 must produce exactly one TIMA
// increment (the falling edge of the enable AND-gate); the stub just
// latches the register, so fast poll loops that toggle TAC under-count.
struct DisableTimer {
    uint16_t cnt = 0;
    uint8_t tima = 0;
    uint8_t tac = 0;

    static int shift_of(uint8_t t) {
        return (t & 0x03) == 0 ? 9 : (t & 0x03) == 1 ? 3
               : (t & 0x03) == 2 ? 5 : 7;
    }

    [[nodiscard]] bool selected_bit() const {
        return ((cnt >> shift_of(tac)) & 1u) != 0;
    }

    void step(int cycles) {
        const bool en = (tac & 0x04) != 0;
        for (int blocks = cycles / 4; blocks > 0; --blocks) {
            cnt = static_cast<uint16_t>(cnt + 4);
            const bool cur = selected_bit();
            if (en && prev_bit && !cur) ++tima;
            prev_bit = cur;
        }
    }

    void write_tac(uint8_t value) {  // TODO(2): seeded bug: no disable edge
        tac = static_cast<uint8_t>(value & 0x07);
    }

    [[nodiscard]] bool enabled() const { return (tac & 0x04) != 0; }
    bool prev_bit = false;
};
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// ---- excerpt C: overflow reload -------------------------------------
// Correct version: on the 0xFF->0x00 wrap TIMA reloads from TMA EXACTLY
// ONCE and raises the interrupt strobe. (Real hardware takes 4 extra
// T-cycles; this excerpt collapses the window like exercise 03.)
struct ReloadTimer {
    uint16_t cnt = 0;
    uint8_t tima = 0;
    uint8_t tma = 0;
    uint8_t tac = 0;
    bool irq_strobe = false;

    void step(int cycles) {
        for (int blocks = cycles / 4; blocks > 0; --blocks) {
            cnt = static_cast<uint16_t>(cnt + 4);
            const int shift = (tac & 0x03) == 0 ? 9 : (tac & 0x03) == 1 ? 3
                              : (tac & 0x03) == 2 ? 5 : 7;
            const bool en = (tac & 0x04) != 0;
            const bool cur = ((cnt >> shift) & 1u) != 0;
            if (en && prev_bit && !cur) {
                if (++tima == 0x00) {
                    tima = tma;      // reload once...
                    irq_strobe = true;  // ...and raise IF bit 2
                }
            }
            prev_bit = cur;
        }
    }

    bool prev_bit = false;
};
//@LABS-STUB
// TODO(3): find and fix the seeded defect in this overflow path.
// Symptom of defect 3: after every overflow TIMA reads $00 instead of TMA,
// so periodic ISRs drift apart and any game computing its tick period from
// TIMA measures double. The stub reloads from TMA and then ALSO wraps to
// zero (the "double reload").
struct ReloadTimer {
    uint16_t cnt = 0;
    uint8_t tima = 0;
    uint8_t tma = 0;
    uint8_t tac = 0;
    bool irq_strobe = false;

    void step(int cycles) {  // TODO(3)
        for (int blocks = cycles / 4; blocks > 0; --blocks) {
            cnt = static_cast<uint16_t>(cnt + 4);
            const int shift = (tac & 0x03) == 0 ? 9 : (tac & 0x03) == 1 ? 3
                              : (tac & 0x03) == 2 ? 5 : 7;
            const bool en = (tac & 0x04) != 0;
            const bool cur = ((cnt >> shift) & 1u) != 0;
            if (en && prev_bit && !cur) {
                if (++tima == 0x00) {
                    tima = tma;      // reload once...
                    tima = 0x00;     // seeded bug: ...and then clobber to $00
                    irq_strobe = true;
                }
            }
            prev_bit = cur;
        }
    }

    bool prev_bit = false;
};
//@LABS-END

}  // namespace gbdbg
