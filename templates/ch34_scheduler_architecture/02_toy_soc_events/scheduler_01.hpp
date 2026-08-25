#pragma once
// Vendored copy of the ch34/01_event_queue reference Scheduler (solution).
// Exercise 02 builds on the finished scheduler; students who completed
// 01 already own this code.
#include <cstdint>
#include <functional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace sched {

struct Event {
    uint64_t timestamp = 0;
    uint64_t seq = 0;
    std::string name;
    std::function<void()> fire;

    bool operator>(const Event& rhs) const {
        if (timestamp != rhs.timestamp) return timestamp > rhs.timestamp;
        return seq > rhs.seq;
    }
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

// Short alias used by soc.hpp.
using Sched = sched::Scheduler;
