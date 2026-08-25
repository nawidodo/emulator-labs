#pragma once
// Debug variant of the ch34/01 scheduler. Same public API, same Event
// struct — but the ordering comparator carries a seeded defect.
#include <cstdint>
#include <functional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace sched {

struct Event {
    uint64_t timestamp = 0;
    uint64_t seq = 0;  // insertion counter (the FIFO tie-break)
    std::string name;
    std::function<void()> fire;

//@LABS-BEGIN 1
//@LABS-SOLUTION
    // Earliest first; ties broken by insertion seq so equal-timestamp
    // events dispatch FIFO, exactly like hardware request lines.
    bool operator>(const Event& rhs) const {
        if (timestamp != rhs.timestamp) return timestamp > rhs.timestamp;
        return seq > rhs.seq;
    }
//@LABS-STUB
    // BUG: the tie-break is missing. When timestamps are equal the
    // comparator returns false for both orderings, and std::priority_queue
    // resolves ties by heap layout — which depends on push history, NOT on
    // insertion order. TODO(1): restore the seq-based FIFO tie-break.
    bool operator>(const Event& rhs) const { return timestamp > rhs.timestamp; }
//@LABS-END
};

class Scheduler {
public:
    void schedule(uint64_t timestamp, std::function<void()> fn,
                  const char* name = "event") {
        queue_.push(Event{timestamp, next_seq_++, name, std::move(fn)});
    }

    bool step() {
        if (queue_.empty()) return false;
        Event ev = queue_.top();
        queue_.pop();
        now_ = ev.timestamp;
        if (ev.fire) ev.fire();
        return true;
    }

    void run_until(uint64_t limit) {
        while (!queue_.empty() && queue_.top().timestamp <= limit) step();
    }

    uint64_t now() const { return now_; }
    size_t pending() const { return queue_.size(); }

private:
    std::priority_queue<Event, std::vector<Event>, std::greater<Event>>
        queue_;
    uint64_t now_ = 0;
    uint64_t next_seq_ = 0;
};

}  // namespace sched
