#pragma once
// GBA timers: prescaled 16-bit up-counters with exact reload semantics and
// cascade chaining. Deterministic closed-form tick accounting (a production
// core would schedule overflow events instead — see ex.05).
#include <cstdint>

namespace gba {

using u16 = uint16_t;
using u32 = uint32_t;
using s32 = int32_t;
using u64 = uint64_t;

struct Timer {
    u16 counter = 0;
    u16 reload = 0;
    u16 control = 0;

    bool enable() const { return (control >> 7) & 1; }
    bool cascade() const { return (control >> 2) & 1; }
    bool irq_en() const { return (control >> 6) & 1; }
    int prescale_field() const { return control & 3; }
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Cycles per timer tick for the prescaler field: 0 -> 1, 1 -> 64,
// 2 -> 256, 3 -> 1024.
inline u64 prescale_cycles(int prescaler_field) {
    switch (prescaler_field & 3) {
        case 0: return 1;
        case 1: return 64;
        case 2: return 256;
        default: return 1024;
    }
}
//@LABS-STUB
// TODO(1): map the 2-bit prescaler field to cycles per tick:
// 0 -> 1 cycle, 1 -> 64, 2 -> 256, 3 -> 1024.
inline u64 prescale_cycles(int prescaler_field) {
    (void)prescaler_field;
    return 0;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Apply exactly `ticks` increments to a timer (closed form). Returns the
// number of overflows. The counter counts up and wraps to `reload` at
// 0x10000, so a freshly reloaded timer overflows every
// `0x10000 - reload` ticks.
inline int timer_advance_ticks(Timer& t, u64 ticks) {
    if (ticks == 0) return 0;
    u64 until_ovf = 0x10000ull - t.counter;
    if (ticks < until_ovf) {
        t.counter = u16(t.counter + ticks);
        return 0;
    }
    u64 remaining = ticks - until_ovf;
    u64 period = 0x10000ull - t.reload;
    u64 overflows = 1 + remaining / period;
    t.counter = u16(t.reload + (remaining % period));
    return int(overflows > 0xFFFFFFFFull ? 0xFFFFFFFFull : overflows);
}

// Advance a free-running (non-cascading) enabled timer by `cycles`.
// Returns overflow count; leftover sub-tick cycles come back via `*rem`.
inline int timer_tick(Timer& t, u64 cycles, u64* rem) {
    *rem = 0;
    if (!t.enable() || t.cascade()) return 0;
    u64 ps = prescale_cycles(t.prescale_field());
    *rem = cycles % ps;
    return timer_advance_ticks(t, cycles / ps);
}
//@LABS-STUB
// TODO(2): implement both functions. `timer_advance_ticks` adds exactly
// `ticks` counts with wrap-to-reload at 0x10000 (closed form is fine);
// `timer_tick` converts guest cycles into prescaled ticks first and reports
// leftover cycles via `*rem`. Disabled or cascading timers do not run here.
inline int timer_advance_ticks(Timer& t, u64 ticks) {
    (void)t;
    (void)ticks;
    return 0;  // wrong on purpose
}
inline int timer_tick(Timer& t, u64 cycles, u64* rem) {
    (void)t;
    (void)cycles;
    (void)rem;
    return 0;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// Cascade chain: timer n-1's overflows become the tick budget of timer n
// when its cascade bit is set (prescaler bypassed); non-cascading timers
// consume guest cycles directly. IRQ flag bit n set when timer n overflows
// with its IRQ bit on. Returns the final timer's overflow count.
inline int chain_tick(Timer (&t)[4], u64 cycles, u16& irq_flags) {
    irq_flags = 0;
    int last_overflows = 0;
    u64 budget = cycles;
    for (int i = 0; i < 4; ++i) {
        if (!t[i].enable() || budget == 0) break;
        int ovf;
        if (i > 0 && t[i].cascade()) {
            ovf = timer_advance_ticks(t[i], budget);
        } else {
            u64 rem = 0;
            ovf = timer_tick(t[i], budget, &rem);
            budget -= rem;  // keep whole-tick alignment for the next stage
        }
        if (ovf && t[i].irq_en()) irq_flags |= u16(1 << i);
        last_overflows = ovf;
        budget = u64(ovf);
    }
    return last_overflows;
}
//@LABS-STUB
// TODO(3): wire four timers in cascade. Timer n-1's overflow count feeds
// timer n as ticks when timer n has its cascade bit set (its own
// prescaler is ignored); otherwise it runs on guest cycles directly. Set
// IRQ flag bits for overflowing timers and return the last stage's count.
inline int chain_tick(Timer (&t)[4], u64 cycles, u16& irq_flags) {
    (void)t;
    (void)cycles;
    irq_flags = 0;
    return 0;  // wrong on purpose
}
//@LABS-END

}  // namespace gba
