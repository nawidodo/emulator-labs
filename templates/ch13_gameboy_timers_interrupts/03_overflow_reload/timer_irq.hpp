#pragma once
#include <cstdint>

#include "../01_divider/int_ctl.hpp"
#include "../02_tima_edge/timer_dev.hpp"

namespace gb {

// Exercise 13.03 -- overflow policy. Exercise 02's TIMA wraps to $00 and
// pulses; real hardware reloads TMA and raises the timer interrupt line,
// but takes 4 extra T-cycles doing so (TIMA reads $00 during that window).
// We implement the IMMEDIATE reload -- a documented deterministic
// simplification; see LECTURE.md ("the missing four cycles").

constexpr uint8_t kTimerIf = IntCtl::kBits[2];  // IF bit 2: timer line

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Consume one overflow pulse: reload TIMA from TMA immediately and raise
// IF bit 2. On hardware the reload completes 4 T-cycles after the wrap
// (during which TIMA reads $00); we collapse that window to zero and say
// so everywhere.
inline void settle_overflow(TimerDevice& t, IntCtl& ctl) {
    if (!t.overflow_pulse) return;
    t.overflow_pulse = false;
    t.tima = t.tma;              // immediate reload (HW delays 4 T-cycles)
    ctl.flags = static_cast<uint8_t>(ctl.flags | kTimerIf);
}
//@LABS-STUB
// TODO(1): on an overflow pulse, reload TIMA from TMA immediately, raise
// IF bit 2 ($FF0F), and clear the pulse.
inline void settle_overflow(TimerDevice& t, IntCtl& ctl) {
    (void)t;
    (void)ctl;
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Machine-side tick: advance the timer by whole 4-T-cycle blocks, then
// apply the overflow policy. Returns true when the timer overflowed during
// this call so drivers can log the event.
inline bool timer_tick(TimerDevice& t, IntCtl& ctl, int cycles) {
    t.step(cycles);
    const bool fired = t.overflow_pulse;
    settle_overflow(t, ctl);
    return fired;
}
//@LABS-STUB
// TODO(2): step the timer by `cycles` and settle any overflow afterwards;
// report whether an overflow happened.
inline bool timer_tick(TimerDevice& t, IntCtl& ctl, int cycles) {
    (void)t;
    (void)ctl;
    (void)cycles;
    return false;
}
//@LABS-END

}  // namespace gb
