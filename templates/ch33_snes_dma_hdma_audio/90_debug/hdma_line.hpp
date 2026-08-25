#pragma once
// Debugging exercise (chapter 33, 90_debug): a scanline gradient engine.
//
// The INIDISP register ($2100, brightness in bits 0-3) is updated once per
// visible scanline through HDMA so the screen shows a vertical gradient.
// Each table entry is one line: header $01 ("fresh data this line") plus
// the brightness byte.
//
// THE BUG (shipped in the stub side): the effect lands ONE SCANLINE LATE.
// Line N's entry is applied at the END of line N instead of the START, so
// line N+1 is the first line that renders with it -- and line 0 renders
// before any write ever happened.
#include <cstdint>
#include <span>
#include <vector>

namespace snesdma::debug {

inline constexpr int kLines = 224;
inline constexpr uint16_t kRegInidisp = 0x2100;

// One recorded HDMA register write: which scanline it took effect on and
// what value landed in INIDISP.
struct LineWrite {
    int line = 0;
    uint8_t value = 0;
};

class GradientHdma {
public:
    void load_table(std::span<const uint8_t> per_line_values) {
        table_.assign(per_line_values.begin(), per_line_values.end());
    }

    // Runs all 224 scanlines and returns the full register-write log.
    // Contract: the value for line n must appear WITH line n in the log --
    // effects apply at line START (see hdma.hpp exercise 02).
    //@LABS-BEGIN 1
    //@LABS-SOLUTION
    std::vector<LineWrite> run() {
        std::vector<LineWrite> log;
        log.reserve(size_t(kLines));
        for (int n = 0; n < kLines; ++n) {
            // Correct: line n's entry is applied at line n's START, so the
            // write is recorded against line n itself.
            log.push_back({n, table_[size_t(n)]});
        }
        return log;
    }
    //@LABS-STUB
    // TODO(1): apply line n's table entry AT THE START of line n, i.e.
    // record {n, table[n]} for every n in [0, 224). The code below applies
    // each entry at the END of its line instead (classic off-by-one
    // scanline bug): line 0 never sees its value and every later line
    // renders with the previous line's data.
    std::vector<LineWrite> run() {
        std::vector<LineWrite> log;
        log.reserve(size_t(kLines));
        for (int n = 0; n < kLines; ++n) {
            if (n == 0) continue;               // BUG: line 0 skipped
            log.push_back({n, table_[size_t(n) - 1]});  // BUG: stale by one
        }
        return log;
    }
    //@LABS-END

private:
    std::vector<uint8_t> table_;
};

}  // namespace snesdma::debug
