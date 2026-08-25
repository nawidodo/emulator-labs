#pragma once
// Event-driven replacement for legacy::LegacyTimer (ch13 interface).
// Instead of being ticked every cycle it publishes its next underflow as
// an absolute deadline on the master clock and applies catch-up fires
// when serviced late.
//
// Semantics mirrored bit-exactly from legacy_timer.hpp:
//   - counter reloads from the STORED period at each underflow,
//   - write_period() mid-count only affects the NEXT reload,
//   - write_period(0) freezes the timer (counter keeps its value);
//     a later write_period(p>0) resumes counting from the frozen value.
#include <cstdint>

namespace challenge {

inline constexpr uint64_t kNoDeadline = ~uint64_t{0};

class EventTimer {
public:
    // Power-on / explicit re-arm: full period counts down from `now`.
    void arm(uint64_t now, uint16_t p) {
        period_ = p;
        running_ = p != 0;
        deadline_ = now + p;
        frozen_remaining_ = 0;
    }

    //@LABS-BEGIN 1
    //@LABS-SOLUTION
    uint64_t next_event() const {
        return running_ ? deadline_ : kNoDeadline;
    }
    //@LABS-STUB
    // TODO(1): return the absolute underflow deadline while running,
    // kNoDeadline when frozen/stopped.
    uint64_t next_event() const { return kNoDeadline; }  // wrong on purpose
    //@LABS-END

    //@LABS-BEGIN 2
    //@LABS-SOLUTION
    void write_period(uint16_t q, uint64_t now) {
        if (q == 0) {
            if (running_) {
                // Freeze: remember how much of the count remained so a
                // later write resumes exactly where silicon would.
                frozen_remaining_ = deadline_ > now ? deadline_ - now : 0;
                running_ = false;
            }
            period_ = 0;
            return;
        }
        period_ = q;  // takes effect at the NEXT reload (or on resume)
        if (!running_) {
            running_ = true;
            deadline_ = now + frozen_remaining_;
            frozen_remaining_ = 0;
        }
    }
    //@LABS-STUB
    // TODO(2): mirror the legacy set_period semantics. q == 0 freezes the
    // timer (record the remaining count first!). q > 0 becomes the reload
    // value; if the timer was frozen it resumes counting from the frozen
    // remainder starting at `now`.
    void write_period(uint16_t, uint64_t) {
        // wrong on purpose: period writes are dropped
    }
    //@LABS-END

    //@LABS-BEGIN 3
    //@LABS-SOLUTION
    void advance_to(uint64_t now) {
        if (!running_) return;
        // Catch-up: apply EVERY underflow due in (last service, now],
        // in order, reloading from the current stored period each time —
        // exactly what the tick-driven counter would have done.
        while (now >= deadline_) {
            flag_ = true;
            ++fire_count_;
            deadline_ += period_;
        }
    }
    //@LABS-STUB
    // TODO(3): catch-up loop. While now >= deadline_: raise the flag,
    // bump fire_count_, and push the deadline forward by ONE period per
    // underflow (never now + period — that stretches time).
    void advance_to(uint64_t) {
        // wrong on purpose: late service loses underflows
    }
    //@LABS-END

    bool flag() const { return flag_; }
    void clear_flag() { flag_ = false; }
    uint64_t fire_count() const { return fire_count_; }

private:
    uint64_t period_ = 0;
    uint64_t deadline_ = 0;
    uint64_t frozen_remaining_ = 0;
    uint64_t fire_count_ = 0;
    bool running_ = false;
    bool flag_ = false;
};

}  // namespace challenge
