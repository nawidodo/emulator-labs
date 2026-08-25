#pragma once
//
// ch40 / timers.hpp — PS1 root counters 0..2
// (psx-spx section "Timers", ports 1F801100h + n*10h).
//
// Each timer owns three 16-bit registers:
//
//   +0h COUNTER  current value (R/W), forced to 0 by a MODE write
//   +4h MODE     configuration + readable event flags (see bit layout)
//   +8h TARGET   compare value
//
// MODE bit layout follows psx-spx exactly:
//
//   0      sync enable        (0 = free run)
//   1-2    sync mode          (per-timer meaning)
//   3      reset after target (0 = wrap at FFFFh, 1 = restart past target)
//   4      IRQ on target
//   5      IRQ on FFFFh wrap
//   6      IRQ repeat         (0 = one-shot until next MODE write)
//   7      IRQ pulse/toggle
//   8-9    clock source       (per-timer table, see ClockSource)
//   10     interrupt request  (set after MODE write; flips in toggle mode)
//   11     reached target     (sticky, cleared by reading MODE)
//   12     reached FFFF       (sticky, cleared by reading MODE)
//
// The environment calls tick() once per system clock with that cycle's GPU
// timing signals; everything is integer math and fully deterministic.

#include <cstdint>

namespace ps1::sysdev {

constexpr int kTimerCount = 3;

// ---- MODE field accessors ------------------------------------------------

inline bool     mode_sync_enable(uint16_t m) { return m & 1u; }
inline unsigned mode_sync_mode(uint16_t m) { return (m >> 1) & 3u; }
inline bool     mode_reset_after_target(uint16_t m) { return (m >> 3) & 1u; }
inline bool     mode_irq_on_target(uint16_t m) { return (m >> 4) & 1u; }
inline bool     mode_irq_on_wrap(uint16_t m) { return (m >> 5) & 1u; }
inline bool     mode_irq_repeat(uint16_t m) { return (m >> 6) & 1u; }
inline bool     mode_irq_toggle(uint16_t m) { return (m >> 7) & 1u; }
inline unsigned mode_clock_source(uint16_t m) { return (m >> 8) & 3u; }

constexpr uint16_t kModeFlagIrqRequest    = 1u << 10;
constexpr uint16_t kModeFlagReachedTarget = 1u << 11;
constexpr uint16_t kModeFlagReachedWrap   = 1u << 12;

// Effective clock source for a given timer index + MODE bits 8-9.
// Timer 0: sysclk, dotclock(pixel), sysclk/8, hblank.
// Timer 1: sysclk, hblank, sysclk/8, sysclk/8.
// Timer 2: sysclk, sysclk/8, sysclk/8, sysclk/8.
enum class ClockSource { Sysclk, Dot, Sysclk8, Hblank };

inline ClockSource clock_source(int n, uint16_t mode) {
    const unsigned sel = mode_clock_source(mode);
    switch (n) {
        case 0:
            return sel == 0 ? ClockSource::Sysclk
                 : sel == 1 ? ClockSource::Dot
                 : sel == 2 ? ClockSource::Sysclk8
                            : ClockSource::Hblank;
        case 1:
            return sel == 0 ? ClockSource::Sysclk
                 : sel == 1 ? ClockSource::Hblank
                            : ClockSource::Sysclk8;
        default:
            return sel == 0 ? ClockSource::Sysclk : ClockSource::Sysclk8;
    }
}

struct TimerSignals {
    // One-cycle pulses (true for exactly one tick() call).
    bool dot_pulse = false;
    bool hblank_pulse = false;
    // Levels during this cycle (driven by GPU timing).
    bool hblank_level = false;
    bool vblank_level = false;
};

class TimerBank {
public:
    struct Regs {
        uint16_t counter = 0;
        uint16_t mode = 0;
        uint16_t target = 0;
    };

    // Interrupt sink: called whenever a timer's output line changes state;
    // `asserted` is the new level. Pulse-mode events assert then deassert,
    // toggle-mode events alternate high/low every second event.
    using IrqSink = void (*)(void* user, int timer, bool asserted);

    Regs regs[kTimerCount];

    // ---- Register file ----------------------------------------------------

    uint16_t read_counter(int n) { return regs[n].counter; }

    void write_counter(int n, uint16_t v) { regs[n].counter = v; }

    void write_target(int n, uint16_t v) { regs[n].target = v; }

    void write_mode(int n, uint16_t v) {
        auto& r = regs[n];
        r.mode = static_cast<uint16_t>(v & 0x1FFFu);
        r.counter = 0;                     // hardware forces a counter reset
        divider_ = 0;
        sync_wait_ = true;                 // sync mode 3 arms on first blank
        one_shot_spent_ = false;
        irq_request_ = true;               // bit 10: "no request" after write
        reached_target_ = false;
        reached_wrap_ = false;
        prev_blank_[0] = prev_blank_[1] = false;
    }

    uint16_t read_mode(int n) {
        auto& r = regs[n];
        // Bit 10 is state, not storage: it reads "no request" after a MODE
        // write and flips per event in toggle mode.
        uint16_t v = static_cast<uint16_t>(r.mode & ~kModeFlagIrqRequest);
        if (irq_request_) v |= kModeFlagIrqRequest;
        if (reached_target_) v |= kModeFlagReachedTarget;
        if (reached_wrap_) v |= kModeFlagReachedWrap;
        reached_target_ = false;           // sticky flags clear on READ
        reached_wrap_ = false;
        return v;
    }

    bool irq_line(int n) const {
        (void)n;
        return !irq_request_;              // line asserted while requesting
    }

    // ---- Stepping ----------------------------------------------------------

    // Advance all three root counters by one system-clock cycle. The sink
    // observes every IRQ line transition (rising AND falling).
    void tick(const TimerSignals& s, IrqSink sink, void* user) {
        const bool hb_rise = s.hblank_level && !prev_blank_[0];
        const bool vb_rise = s.vblank_level && !prev_blank_[1];
        prev_blank_[0] = s.hblank_level;
        prev_blank_[1] = s.vblank_level;
        for (int n = 0; n < kTimerCount; ++n) {
            apply_sync(n, hb_rise, vb_rise);
            if (!counts_this_cycle(n, s)) continue;
            step_counter(n);
            deliver_irq(n, sink, user);
        }
    }

private:
    void apply_sync(int n, bool hb_rise, bool vb_rise) {
        const uint16_t m = regs[n].mode;
        if (!mode_sync_enable(m) || n == 2) return;   // timer 2 has no video
        const unsigned sm = mode_sync_mode(m);
        const bool rising = (n == 0) ? hb_rise : vb_rise;
        switch (sm) {
            case 1:                        // reset at every blank start
            case 2:                        // ... and count inside only
                if (rising) regs[n].counter = 0;
                break;
            case 3:                        // wait for one blank, free-run
                if (sync_wait_ && rising) sync_wait_ = false;
                break;
            default:
                break;
        }
    }

    bool counts_this_cycle(int n, const TimerSignals& s) {
        const uint16_t m = regs[n].mode;
        if (mode_sync_enable(m)) {
            const unsigned sm = mode_sync_mode(m);
            if (n == 2) {
                // Root counter 2 has no video sync: modes 0/3 stop forever,
                // modes 1/2 behave like free run.
                if (sm == 0 || sm == 3) return false;
            } else {
                const bool level =
                    (n == 0) ? s.hblank_level : s.vblank_level;
                switch (sm) {
                    case 0: if (level) return false; break;   // pause in blank
                    case 2: if (!level) return false; break;  // count in blank
                    case 3: if (sync_wait_) return false; break;
                    default: break;                           // 1: always
                }
            }
        }
        switch (clock_source(n, m)) {
            case ClockSource::Dot:    return s.dot_pulse;
            case ClockSource::Hblank: return s.hblank_pulse;
            case ClockSource::Sysclk8:
                if (++divider_ >= 8) { divider_ = 0; return true; }
                return false;
            default:                  return true;     // Sysclk
        }
    }

    void step_counter(int n) {
        auto& r = regs[n];
        uint32_t c = static_cast<uint32_t>(r.counter) + 1;
        const bool wrapped = c > 0xFFFFu;
        if (wrapped) c = 0;
        if (!wrapped && r.target != 0 &&
            static_cast<uint16_t>(c) == r.target) {
            // Counter reaches the target value (inclusive): fire, and if
            // MODE bit 3 says so, restart from zero on this very tick.
            reached_target_ = true;
            target_event_ = true;
            if (mode_reset_after_target(r.mode)) c = 0;
        } else if (wrapped) {
            // Passed target and rolled over FFFFh in the same increment.
            reached_wrap_ = true;
            wrap_event_ = true;
        }
        r.counter = static_cast<uint16_t>(c);
    }

    void deliver_irq(int n, IrqSink sink, void* user) {
        const uint16_t m = regs[n].mode;
        const bool qualifies =
            (target_event_ && mode_irq_on_target(m)) ||
            (wrap_event_ && mode_irq_on_wrap(m));
        target_event_ = false;
        wrap_event_ = false;
        if (!qualifies) return;
        if (!mode_irq_repeat(m) && one_shot_spent_) return;  // one-shot gate
        one_shot_spent_ = true;
        if (mode_irq_toggle(m)) {
            // Toggle mode: bit 10 flips per event, the line asserts on the
            // 1 -> 0 transition, so every SECOND event reaches the
            // interrupt controller as a rising edge.
            irq_request_ = !irq_request_;
            if (sink) sink(user, n, !irq_request_);
        } else {
            // Pulse mode: short pulse per event, bit 10 returns to idle.
            if (sink) {
                sink(user, n, true);
                sink(user, n, false);
            }
        }
    }

    uint8_t divider_ = 0;            // sysclk/8 phase, reset by MODE writes
    bool sync_wait_ = false;         // sync mode 3: armed until first blank
    bool prev_blank_[2] = {false, false};  // hblank / vblank level history
    bool one_shot_spent_ = false;    // one-shot latch, rearmed by MODE write
    bool irq_request_ = true;        // MODE bit 10 state (false = requesting)
    bool reached_target_ = false;    // sticky until MODE is READ
    bool reached_wrap_ = false;      // sticky until MODE is READ
    bool target_event_ = false;      // set by step_counter, consumed by IRQ
    bool wrap_event_ = false;
};

}  // namespace ps1::sysdev
