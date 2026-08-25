#pragma once
// Trace logger with filters + capped instruction history ring (§54).
// The trace is the ground truth for "trace-first debugging": run the
// reference and yours, diff, fix the FIRST divergence.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>


namespace tools {

struct TraceRecord {
    uint8_t pc = 0;
    uint8_t op = 0;
    uint8_t a = 0;   // A after execution
    uint64_t cyc = 0;
};

// Canonical line format (compare_trace.py-compatible key=value tokens):
//   pc=04 op=04 a=05 cyc=11
inline std::string format(const TraceRecord& r) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "pc=%02X op=%02X a=%02X cyc=%llu",
                  r.pc, r.op, r.a, static_cast<unsigned long long>(r.cyc));
    return buf;
}

//@LABS-BEGIN 1
//@LABS-SOLUTION
class HistoryRing {
public:
    explicit HistoryRing(size_t capacity)
        : slots_(capacity ? capacity : 1) {}

    void push(const TraceRecord& r) {
        slots_[head_] = r;
        head_ = (head_ + 1) % slots_.size();
        if (count_ < slots_.size()) ++count_;
    }

    // n = how many entries back (0 = newest); nullptr when out of range.
    const TraceRecord* at(size_t n) const {
        if (n >= count_) return nullptr;
        const size_t idx = (head_ + slots_.size() - 1 - n) % slots_.size();
        return &slots_[idx];
    }

    size_t size() const { return count_; }
    size_t capacity() const { return slots_.size(); }

private:
    std::vector<TraceRecord> slots_;
    size_t head_ = 0;
    size_t count_ = 0;
};
//@LABS-STUB
class HistoryRing {
public:
    explicit HistoryRing(size_t capacity) { (void)capacity; }
    // TODO(1): fixed-capacity ring — push overwrites the oldest entry;
    // at(n) returns the record n captures back (nullptr out of range).
    void push(const TraceRecord&) {}
    const TraceRecord* at(size_t) const { return nullptr; }
    size_t size() const { return 0; }
    size_t capacity() const { return 0; }
};
//@LABS-END

class TraceLogger {
public:
    struct Filter {
        bool by_op_range = false;
        uint8_t op_lo = 0, op_hi = 0xFF;
        bool by_pc = false;
        uint8_t pc = 0;

//@LABS-BEGIN 2
//@LABS-SOLUTION
        bool matches(const TraceRecord& r) const {
            // Both filters AND together; bounds are INCLUSIVE.
            if (by_op_range && (r.op < op_lo || r.op > op_hi)) return false;
            if (by_pc && r.pc != pc) return false;
            return true;
        }
//@LABS-STUB
        bool matches(const TraceRecord&) const {
            // TODO(2): apply BOTH filters — inclusive opcode range and
            // exact executed-pc match.
            return true;  // wrong on purpose: filter keeps everything
        }
//@LABS-END
    };

    explicit TraceLogger(Filter f) : filter_(f) {}

    void log(const TraceRecord& r) {
        ++seen_;
        if (filter_.matches(r)) kept_.push_back(r);
    }

    const std::vector<TraceRecord>& kept() const { return kept_; }
    uint64_t seen() const { return seen_; }

private:
    Filter filter_;
    std::vector<TraceRecord> kept_;
    uint64_t seen_ = 0;
};

}  // namespace tools
