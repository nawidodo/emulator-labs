#pragma once
//
// ch45 / 90_debug — SEEDED BUGS in a CD-ROM seek path.
// Tests run RED until both bugs are fixed. Write bug-report.md with
// bug / root cause / first divergence / fix / regression per bug.
//
// Symptoms you will observe:
//   BUG 1: seeks land exactly one LEAD-IN (150 frames) away from the
//          requested sector — audio/data reads come back wrong by a
//          constant offset no matter the target.
//   BUG 2: seek-completion interrupts arrive at the SAME tick as the
//          command — the drive reports "seek done" before any head
//          movement could have happened.

#include <cstdint>
#include <functional>
#include <vector>

namespace cdbg {

class MiniController {
public:
    using Sink = std::function<void(uint64_t t, int32_t lba)>;

    void set_sink(Sink s) { sink_ = std::move(s); }
    int32_t current() const { return cur_; }

//@LABS-BEGIN 1
//@LABS-SOLUTION
    void set_loc_msf(unsigned m, unsigned s, unsigned f) {
        // MSF -> LBA must exclude the 150-frame lead-in.
        target_ = static_cast<int32_t>((m * 60 + s) * 75 + f) - 150;
        pending_ = true;
    }
//@LABS-STUB
    // TODO(1): symptom above — one constant offset on every seek. Find
    // which documented conversion step is missing here.
    void set_loc_msf(unsigned m, unsigned s, unsigned f) {
        target_ = static_cast<int32_t>((m * 60 + s) * 75 + f);
        pending_ = true;
    }
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
    void seek() {
        if (!pending_) return;
        const uint64_t delay = latency();
        events_.push_back({now_ + delay, false});
    }

    void tick(uint64_t n) {
        now_ += n;
        bool again = true;
        while (again) {
            again = false;
            for (auto& e : events_)
                if (!e.done && e.at <= now_) {
                    e.done = true;
                    arrive(e.at);
                    again = true;
                }
        }
    }

private:
    uint64_t latency() const {
        const int64_t d =
            target_ > cur_ ? int64_t(target_ - cur_)
                           : int64_t(cur_ - target_);
        return 100u + static_cast<uint64_t>(d);
    }
    void arrive(uint64_t at) {
        cur_ = target_;               // head snaps to the target sector
        pending_ = false;
        if (sink_) sink_(at, cur_);
    }

    int32_t target_ = 0;
    int32_t cur_ = 0;
    bool pending_ = false;
    struct Evt {
        uint64_t at;
        bool done;
    };
    std::vector<Evt> events_;
    Sink sink_;
    uint64_t now_ = 0;
//@LABS-STUB
    void seek() {
        if (!pending_) return;
        events_.push_back({now_, true});  // BUG 2: zero-latency completion
    }

    void tick(uint64_t n) { now_ += n; pump(); }

    void pump() {
        for (auto& e : events_)
            if (!e.done && e.at <= now_) {
                e.done = true;
                arrive(now_);
            }
    }

    void arrive(uint64_t at) {
        cur_ = target_;
        pending_ = false;
        if (sink_) sink_(at, cur_);
    }

    uint64_t latency() const {
        return 0;  // BUG 2 lives here: no modeled head movement time
    }

    int32_t target_ = 0;
    int32_t cur_ = 0;
    bool pending_ = false;
    struct Evt {
        uint64_t at;
        bool done;
    };
    std::vector<Evt> events_;
    Sink sink_;
    uint64_t now_ = 0;
//@LABS-END
};

}  // namespace cdbg
