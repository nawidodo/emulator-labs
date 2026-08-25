#pragma once
//
// ch40 / 03_scheduler — deterministic event scheduler
//
// Emulator timing must never touch the wall clock: everything is ordered by
// an integer cycle counter and a list of future events. The scheduler fires
// all events due at or before a requested cycle, oldest first, breaking
// same-cycle ties by insertion order — so two runs of the same program
// produce bit-identical schedules.
//
// Periodic behavior (timer ticks, hblank pulses) is built by having an
// event callback reschedule itself before returning.

#include <cstdint>
#include <cstddef>
#include <vector>

namespace ps1::sysdev {

class Scheduler {
public:
    using Callback = void (*)(void* user);

    void schedule(uint64_t at_cycle, int id, Callback fn, void* user) {
        events_.push_back(Event{at_cycle, ++seq_, id, fn, user});
    }

    // Remove every pending event with the given id; true if any was removed.
    bool cancel(int id) {
        bool removed = false;
        for (Event& e : events_) {
            if (!e.fired && e.id == id) {
                e.fired = true;                // keep slots, skip on dispatch
                removed = true;
            }
        }
        return removed;
    }

    // Fire every armed event with at_cycle <= `cycle` in (cycle, insertion)
    // order. Events scheduled DURING dispatch that are already due also
    // fire within this call.
    void run_to(uint64_t cycle) {
        for (;;) {
            Event* best = nullptr;
            for (Event& e : events_) {
                if (e.fired || e.at > cycle) continue;
                if (best == nullptr) {
                    best = &e;
                    continue;
                }
                const bool earlier =
                    e.at != best->at ? e.at < best->at : e.seq < best->seq;
                if (earlier) best = &e;
            }
            if (best == nullptr) break;
            best->fired = true;
            now_ = best->at;
            best->fn(best->user);
        }
        if (now_ < cycle) now_ = cycle;
    }

    uint64_t now() const { return now_; }

    size_t pending() const {
        size_t n = 0;
        for (const Event& e : events_)
            if (!e.fired) ++n;
        return n;
    }

private:
    struct Event {
        uint64_t at;
        uint64_t seq;      // insertion order: breaks same-cycle ties
        int id;
        Callback fn;
        void* user;
        bool fired = false;
    };

    std::vector<Event> events_;
    uint64_t seq_ = 0;
    uint64_t now_ = 0;
};

}  // namespace ps1::sysdev
