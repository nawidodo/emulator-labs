#pragma once
//
// ch45 / 99_coding_test — unseen-spec mini CD sequencer.
// Specification in CODING_TEST.md. Hidden grading replays command
// sequences you have never seen and compares transcripts byte-for-byte.

#include <cstdint>
#include <functional>
#include <sstream>
#include <string>
#include <vector>
#include <cstdio>

namespace cdt {

struct Event {
    uint64_t at;
    unsigned seq;
    bool done = false;
    std::function<void()> fn;
};

class Sequencer {
public:
    void set_log(std::ostream* log) { log_ = log; }

    // ---- guest API ----------------------------------------------------
    void cmd_getstat() {
        emit(3, {static_cast<uint8_t>(stat_)});
    }

//@LABS-BEGIN 1
//@LABS-SOLUTION
    void cmd_setloc(unsigned m, unsigned s, unsigned f) {
        target_ = static_cast<int32_t>((m * 60 + s) * 75 + f) - 150;
        emit(3, {});
    }
//@LABS-STUB
    // TODO(1): store the target LBA (mind the lead-in bias!) and log an
    // INT3 first response with no payload bytes.
    void cmd_setloc(unsigned m, unsigned s, unsigned f) {
        (void)m; (void)s; (void)f;
    }
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
    void cmd_init() {
        stat_ |= 0x02;                       // motor on
        emit(3, {static_cast<uint8_t>(stat_)});
        schedule(1200, [this] { emit(2, {static_cast<uint8_t>(stat_)}); });
    }

    void cmd_pause() {
        reading_ = false;
        ++epoch_;
        stat_ &= ~0x20u;
        emit(3, {static_cast<uint8_t>(stat_)});
        schedule(250, [this] { emit(2, {static_cast<uint8_t>(stat_)}); });
    }
//@LABS-STUB
    // TODO(2): Init sets motor-on, logs INT3 now and INT2 after exactly
    // 1200 ticks. Pause stops any active read (invalidating pending
    // sector deliveries!), clears the read stat bit, INT3 now, INT2
    // after 250 ticks.
    void cmd_init() {}
    void cmd_pause() {}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
    void cmd_readn() {
        const uint64_t delay =
            seek_ticks() + kSectorInterval;   // seek plus one sector time
        reading_ = true;
        stat_ |= 0x20u;
        emit(3, {static_cast<uint8_t>(stat_)});
        chain_read(delay);
    }
//@LABS-STUB
    // TODO(3): set read state + stat bit, INT3 now, then deliver sectors:
    // first after `seek_ticks()` (seek from current position to target,
    // plus nothing else), each subsequent one 50 ticks later. Each
    // delivery: advance current LBA by one, log int=1 with lba= field.
    void cmd_readn() {}
//@LABS-END

    void tick(uint64_t n) {
        now_ += n;
        bool again = true;
        while (again) {
            again = false;
            size_t best = SIZE_MAX;
            for (size_t i = 0; i < events_.size(); ++i)
                if (!events_[i].done && events_[i].at <= now_ &&
                    (best == SIZE_MAX || events_[i].at < events_[best].at ||
                     (events_[i].at == events_[best].at &&
                      events_[i].seq < events_[best].seq)))
                    best = i;
            if (best != SIZE_MAX) {
                auto fn = std::move(events_[best].fn);
                events_[best].done = true;
                fn();
                again = true;
            }
        }
    }

    // ---- introspection for tests --------------------------------------
    uint64_t now() const { return now_; }
    int32_t target() const { return target_; }
    int32_t current() const { return cur_; }
    static constexpr uint64_t kSeekBase = 100;
    static constexpr uint64_t kSectorInterval = 50;

private:
    uint64_t seek_ticks() const {
        const int64_t d =
            target_ > cur_ ? int64_t(target_ - cur_)
                           : int64_t(cur_ - target_);
        return kSeekBase + static_cast<uint64_t>(d);
    }
    void chain_read(uint64_t delay) {
        const unsigned ep = epoch_;
        schedule(delay, [this, ep] {
            if (ep != epoch_) return;
            ++cur_;
            if (log_) {
                char b[64];
                std::snprintf(b, sizeof(b), "t=%llu int=1 resp= lba=%d\n",
                              static_cast<unsigned long long>(now_), cur_);
                *log_ << b;
            }
            if (reading_) chain_read(kSectorInterval);
        });
    }
    void emit(uint8_t lvl, std::vector<uint8_t> resp) {
        if (resp.empty()) resp.push_back(static_cast<uint8_t>(stat_));
        if (!log_) return;
        std::ostringstream ss;
        ss << "t=" << now_ << " int=" << static_cast<unsigned>(lvl)
           << " resp=";
        for (size_t i = 0; i < resp.size(); ++i) {
            char b[4];
            std::snprintf(b, sizeof(b), "%02X", resp[i]);
            ss << (i ? "-" : "") << b;
        }
        ss << "\n";
        *log_ << ss.str();
    }
    void schedule(uint64_t delay, std::function<void()> fn) {
        events_.push_back(
            Event{now_ + delay, ++seq_, false, std::move(fn)});
    }

    std::vector<Event> events_;
    std::ostream* log_ = nullptr;
    uint32_t stat_ = 0;
    bool reading_ = false;
    unsigned epoch_ = 0;
    unsigned seq_ = 0;
    int32_t target_ = 0;
    int32_t cur_ = 0;
    uint64_t now_ = 0;
};

}  // namespace cdt
