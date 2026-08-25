#pragma once
#include <cstdint>
#include <functional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace sched {

// One scheduled event. `timestamp` is an ABSOLUTE master-clock deadline in
// guest cycles (never host time). `seq` is a monotonic insertion counter
// used as the FIFO tie-break for equal timestamps: hardware does not
// reorder same-instant events, and neither may the scheduler.
struct Event {
    uint64_t timestamp = 0;
    uint64_t seq = 0;
    std::string name;
    std::function<void()> fire;

    // Earliest timestamp first; on ties, lowest insertion seq first.
    bool operator>(const Event& rhs) const {
        if (timestamp != rhs.timestamp) return timestamp > rhs.timestamp;
        return seq > rhs.seq;
    }
};

// Priority-queue scheduler over integer guest cycles.
class Scheduler {
public:
    // Register `fn` to dispatch at absolute guest cycle `timestamp`.
    // Must preserve FIFO order among events with identical timestamps.
    void schedule(uint64_t timestamp, std::function<void()> fn,
                  const char* name = "event");

    // Dispatch exactly one event (the earliest, ties FIFO). Advances
    // now() to that event's timestamp before running its callback.
    // Returns false when the queue is empty.
    bool step();

    // Dispatch events while the earliest pending timestamp <= limit.
    // An event scheduled DURING dispatch at ts <= limit still fires in
    // this call (the queue is re-checked each iteration).
    void run_until(uint64_t limit);

    uint64_t now() const { return now_; }
    size_t pending() const { return queue_.size(); }

private:
    std::priority_queue<Event, std::vector<Event>, std::greater<Event>>
        queue_;
    uint64_t now_ = 0;  // master clock: guest cycles of last dispatch
    uint64_t next_seq_ = 0;
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline void Scheduler::schedule(uint64_t timestamp, std::function<void()> fn,
                                const char* name) {
    // Absolute deadline + monotonic seq: the seq gives the stable FIFO
    // tie-break for equal timestamps regardless of heap internals.
    queue_.push(Event{timestamp, next_seq_++, name, std::move(fn)});
}
//@LABS-STUB
// TODO(1): push an Event{timestamp, next_seq_++, name, fn} onto queue_.
// The monotonic seq_ counter is what makes equal-timestamp dispatch FIFO.
// Stub compiles so the suite runs RED until you finish it.
inline void Scheduler::schedule(uint64_t timestamp, std::function<void()> fn,
                                const char* name) {
    (void)timestamp;
    (void)name;
    (void)fn;  // wrong on purpose: drops the event
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
inline bool Scheduler::step() {
    if (queue_.empty()) return false;
    Event ev = queue_.top();
    queue_.pop();
    now_ = ev.timestamp;  // master clock jumps to the deadline
    if (ev.fire) ev.fire();
    return true;
}
//@LABS-STUB
// TODO(2): pop the earliest event, set now_ to its timestamp, then run
// its callback. Return false when the queue is empty. Note: handlers may
// schedule new events; taking the Event by value first keeps us safe.
inline bool Scheduler::step() {
    return false;  // wrong on purpose: never dispatches
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
inline void Scheduler::run_until(uint64_t limit) {
    // Re-check every iteration: handlers may schedule more work at or
    // before `limit`, which must still fire inside this window.
    while (!queue_.empty() && queue_.top().timestamp <= limit) step();
}
//@LABS-STUB
// TODO(3): repeatedly step() while the earliest pending timestamp is
// <= limit. Events scheduled during dispatch at ts <= limit must also
// fire here — re-check the top of the queue after each step.
inline void Scheduler::run_until(uint64_t limit) {
    (void)limit;  // wrong on purpose: runs nothing
}
//@LABS-END

}  // namespace sched
