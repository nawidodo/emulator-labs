#pragma once
// Debug variant of the ch49/01 event scheduler. Same public API and same
// Event struct as sched::Scheduler — but the ordering comparator carries a
// SEEDED DEFECT (see DEBUGGING.md). The mini-system wiring from
// ../02_mini_devices runs unmodified on top of it via its template seam.
#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace ps1dbg {

struct Event {
    uint64_t timestamp = 0;
    uint64_t seq = 0;      // insertion counter (should be the FIFO tie-break)
    uint64_t id = 0;
    std::string name;
    bool cancelled = false;
    std::function<void()> fire;

//@LABS-BEGIN 1
//@LABS-SOLUTION
    // Earliest first; ties broken by insertion seq so equal-timestamp
    // events dispatch FIFO, matching hardware latch order.
    struct Later {
        bool operator()(const Event& a, const Event& b) const {
            if (a.timestamp != b.timestamp) return a.timestamp > b.timestamp;
            return a.seq > b.seq;
        }
    };
//@LABS-STUB
    // BUG: the tie-break is INVERTED. Equal-timestamp events dispatch in
    // REVERSE insertion order — "the most recently scheduled request wins".
    // Real hardware samples interrupt request lines in a fixed arrival
    // order, so this reorders interrupts whenever two deadlines collide.
    // TODO(1): restore the seq-based FIFO tie-break (earlier seq first).
    struct Later {
        bool operator()(const Event& a, const Event& b) const {
            if (a.timestamp != b.timestamp) return a.timestamp > b.timestamp;
            return a.seq < b.seq;
        }
    };
//@LABS-END
};

class Scheduler {
public:
    using Handler = std::function<void()>;

    uint64_t schedule(uint64_t timestamp, Handler fn,
                      const char* name = "event") {
        const uint64_t id = next_id_++;
        heap_.push_back(Event{.timestamp = timestamp,
                              .seq = next_seq_++,
                              .id = id,
                              .name = name,
                              .fire = std::move(fn)});
        std::push_heap(heap_.begin(), heap_.end(), Event::Later{});
        return id;
    }

    bool step() {
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

    void run_until(uint64_t deadline) {
        while (!heap_.empty() && heap_.front().timestamp <= deadline) step();
    }

    uint64_t now() const { return now_; }
    size_t pending() const { return heap_.size(); }
    void clear() { heap_.clear(); now_ = 0; }

private:
    std::vector<Event> heap_;
    uint64_t now_ = 0;
    uint64_t next_seq_ = 0;
    uint64_t next_id_ = 1;
};

}  // namespace ps1dbg
