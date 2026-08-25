#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace sched {

// One scheduled event. `timestamp` is an ABSOLUTE master-clock deadline in
// guest CPU cycles (33.8688 MHz since power-on) — never host time. `seq`
// is a monotonic insertion counter used as the FIFO tie-break for equal
// timestamps: hardware request lines latch in a fixed order, and neither
// may the scheduler reorder same-instant events.
struct Event {
    uint64_t timestamp = 0;
    uint64_t seq = 0;      // insertion counter (the FIFO tie-break)
    uint64_t id = 0;       // returned by schedule(), used by cancel()
    std::string name;
    std::function<void()> fire;
    bool cancelled = false;

//@LABS-BEGIN 1
//@LABS-SOLUTION
    // Min-heap ordering: earliest timestamp first; ties broken by
    // insertion seq so equal-timestamp events dispatch FIFO, exactly like
    // hardware interrupt lines sampled in a fixed order.
    struct Later {
        bool operator()(const Event& a, const Event& b) const {
            if (a.timestamp != b.timestamp) return a.timestamp > b.timestamp;
            return a.seq > b.seq;
        }
    };
//@LABS-STUB
    // BUG: the tie-break is REVERSED — equal-timestamp events dispatch in
    // LIFO order ("most recently scheduled wins"). Real hardware latches
    // same-instant requests in arrival order, so this silently reorders
    // interrupts whenever two deadlines collide. TODO(1): restore the
    // seq-based FIFO tie-break (earlier seq dispatches first).
    struct Later {
        bool operator()(const Event& a, const Event& b) const {
            if (a.timestamp != b.timestamp) return a.timestamp > b.timestamp;
            return a.seq < b.seq;
        }
    };
//@LABS-END
};

// Priority-queue scheduler over integer guest cycles. The heap lives in a
// plain vector so cancel() can scan it; cancelled entries are skipped (and
// discarded) lazily at dispatch time.
class Scheduler {
public:
    using Handler = std::function<void()>;

    uint64_t schedule(uint64_t timestamp, Handler fn, const char* name = "event");
    void cancel(uint64_t id);
    bool step();
    void run_until(uint64_t deadline);

    uint64_t now() const { return now_; }

    // Drop all pending events and rewind the clock (fresh-machine state
    // for a System::reset()); sequence numbering keeps monotonic so ids
    // and tie-break order never repeat across resets.
    void clear() {
        heap_.clear();
        now_ = 0;
    }
    size_t pending() const { return heap_.size(); }

private:
    std::vector<Event> heap_;
    uint64_t now_ = 0;
    uint64_t next_seq_ = 0;
    uint64_t next_id_ = 1;
};

//@LABS-BEGIN 2
//@LABS-SOLUTION
inline uint64_t Scheduler::schedule(uint64_t timestamp, Handler fn,
                                    const char* name) {
    uint64_t id = next_id_++;
    heap_.push_back(Event{timestamp, next_seq_++, id, name, std::move(fn),
                          false});
    std::push_heap(heap_.begin(), heap_.end(), Event::Later{});
    return id;
}
//@LABS-STUB
// TODO(2): append Event{timestamp, next_seq_++, fresh id, name, fn} to the
// heap, restore heap order with push_heap, and return the new event id.
// Returning 0 still compiles so the suite runs RED until you finish it.
inline uint64_t Scheduler::schedule(uint64_t timestamp, Handler fn,
                                    const char* name) {
    (void)timestamp; (void)fn; (void)name;
    return 0;
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
inline void Scheduler::cancel(uint64_t id) {
    for (auto& ev : heap_)
        if (ev.id == id && !ev.cancelled) ev.cancelled = true;
}
//@LABS-STUB
// TODO(3): lazily cancel every pending event with this id — mark it
// cancelled; step() discards it when it surfaces. Heap order must not be
// disturbed (no erase here).
inline void Scheduler::cancel(uint64_t id) {
    (void)id;  // wrong on purpose: cancels nothing
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
inline bool Scheduler::step() {
    while (!heap_.empty() && heap_.front().cancelled) {
        std::pop_heap(heap_.begin(), heap_.end(), Event::Later{});
        heap_.pop_back();
    }
    if (heap_.empty()) return false;
    Event ev = heap_.front();
    std::pop_heap(heap_.begin(), heap_.end(), Event::Later{});
    heap_.pop_back();
    now_ = ev.timestamp;
    if (ev.fire) ev.fire();
    return true;
}
//@LABS-STUB
// TODO(4): pop the earliest non-cancelled event, advance now_ to its
// timestamp, then run its callback; return false once only cancelled
// entries or nothing remain. Handlers may schedule new events (even at
// their own timestamp), so take the Event by value before firing.
inline bool Scheduler::step() {
    return false;  // wrong on purpose: never dispatches
}
//@LABS-END

//@LABS-BEGIN 5
//@LABS-SOLUTION
inline void Scheduler::run_until(uint64_t deadline) {
    while (!heap_.empty() && heap_.front().timestamp <= deadline) step();
}
//@LABS-STUB
// TODO(5): keep stepping while the earliest pending timestamp is
// <= deadline. Events scheduled during dispatch at ts <= deadline must
// fire here too — re-check the heap front after every step.
inline void Scheduler::run_until(uint64_t deadline) {
    (void)deadline;  // wrong on purpose: runs nothing
}
//@LABS-END

}  // namespace sched
