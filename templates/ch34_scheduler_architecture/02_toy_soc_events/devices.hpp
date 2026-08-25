#pragma once
// Event-driven devices for the fx8 SoC. Both devices publish their next
// deadline on the master clock instead of being ticked every cycle.
//
// Catch-up model: a device's fire(now) may be invoked LATE (master clock
// already past the ideal deadline, e.g. because a longer CPU instruction
// straddled it). Devices derive their state from absolute deadlines on
// the master clock — they never keep a private "now".

#include <cstdint>
#include <vector>

namespace soc {

inline constexpr uint64_t kNoDeadline = ~uint64_t{0};

// Periodic interrupt source: raises its flag every `period` cycles.
struct TimerDevice {
    uint64_t period = 10;     // guest cycles between fires
    uint64_t last_fire = 0;   // absolute cycle of most recent fire
    uint64_t fire_count = 0;
    bool flag = false;

    // Absolute cycle of the NEXT deadline (last_fire is the anchor so a
    // late dispatch never stretches time).
    uint64_t next_event() const;

    // Advance to the deadline: raise the flag, bookkeep, and re-anchor
    // last_fire on this deadline (catch-up loops handled by caller).
    void fire(uint64_t now);
};

// Byte transmitter: one byte every `byte_period` cycles once queued.
struct UartDevice {
    uint64_t byte_period = 8;         // guest cycles per transmitted byte
    std::vector<uint8_t> pending;     // queued, not yet sent
    std::vector<uint8_t> transmitted; // completed bytes in send order
    uint64_t busy_until = 0;          // deadline of the in-flight byte
    bool busy = false;

    void push(uint8_t b) { pending.push_back(b); }

    // Absolute cycle when the current (or next queued) byte completes;
    // kNoDeadline when idle with nothing pending.
    uint64_t next_event() const;

    // Complete one byte transmission at deadline `now`.
    void fire(uint64_t now);
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline uint64_t TimerDevice::next_event() const {
    return last_fire + period;  // anchored to last deadline, never to "now"
}
//@LABS-STUB
// TODO(1): return the ABSOLUTE cycle of the timer's next deadline. The
// anchor is last_fire + period. Returning now + period here would stretch
// time whenever dispatch runs late.
inline uint64_t TimerDevice::next_event() const {
    return kNoDeadline;  // wrong on purpose: timer never fires
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
inline void TimerDevice::fire(uint64_t now) {
    flag = true;
    ++fire_count;
    last_fire = now;  // re-anchor exactly on the dispatched deadline
}
//@LABS-STUB
// TODO(2): raise flag, bump fire_count, and set last_fire = now so the
// next deadline stays aligned with the master clock.
inline void TimerDevice::fire(uint64_t) {
    // wrong on purpose: no observable effect
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
inline uint64_t UartDevice::next_event() const {
    if (!busy && pending.empty()) return kNoDeadline;
    return busy_until;  // deadline of the byte being transmitted
}
//@LABS-STUB
// TODO(3): if transmitting or holding pending bytes, return busy_until;
// otherwise return kNoDeadline.
inline uint64_t UartDevice::next_event() const {
    return kNoDeadline;
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
inline void UartDevice::fire(uint64_t now) {
    transmitted.push_back(pending.front());
    pending.erase(pending.begin());
    if (!pending.empty()) {
        busy = true;
        busy_until = now + byte_period;  // next byte back-to-back
    } else {
        busy = false;  // line goes idle until the CPU queues again
    }
}
//@LABS-STUB
// TODO(4): move the front pending byte into `transmitted`. If more bytes
// remain, stay busy and set busy_until = now + byte_period; otherwise go
// idle.
inline void UartDevice::fire(uint64_t) {
    // wrong on purpose: bytes never complete transmission
}
//@LABS-END

}  // namespace soc
