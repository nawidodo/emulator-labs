#pragma once
// Exercise 90 — debugging exercise: SEEDED BUG lives in the STUB side of the
// @LABS block below; the SOLUTION side is the corrected code.
//
// The module implements the Mode 1 composition step from exercise 03:
// given the per-pixel candidates of every layer, pick which one reaches the
// screen. The documented rule is: sprites omitted, the winner minimizes
// layer first and then maximizes the priority bit (key
// `layer * 2 + (priority ? 0 : 1)`, smallest value wins), transparent
// candidates already filtered.
//
// See DEBUGGING.md for symptoms and the hint ladder.

#include <cstdint>
#include <span>

namespace snesbus {

struct PixelCandidate {
    uint8_t color = 0;     // tile pixel value (0 = transparent)
    uint8_t palette = 0;
    uint8_t layer = 0;     // 0 = BG1 .. 2 = BG3
    uint8_t priority = 0;  // tilemap priority bit
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Pick the winning candidate: minimize layer first, then maximize the
// priority bit — smallest value of key = layer * 2 + (priority ? 0 : 1)
// wins. BG1 outranks BG2 outranks BG3; within one layer, priority=1 beats
// priority=0. Returns -1 when there is nothing opaque.
inline int compose(std::span<const PixelCandidate> candidates) {
    int best = -1;
    unsigned best_key = 0xFFFFFFFFu;
    for (size_t i = 0; i < candidates.size(); ++i) {
        const unsigned key =
            candidates[i].layer * 2u + (candidates[i].priority != 0 ? 0u : 1u);
        if (key < best_key) {
            best_key = key;
            best = static_cast<int>(i);
        }
    }
    return best;
}
//@LABS-STUB
// Pick the winning candidate. Ranks are computed painter-style so the
// farthest layer sorts first and the nearest ends up on top.
// TODO(1): this passes the transparency tests but games show wrong layers
// overlapping — compare carefully against the documented key rule.
inline int compose(std::span<const PixelCandidate> candidates) {
    int best = -1;
    unsigned best_key = 0;
    for (size_t i = 0; i < candidates.size(); ++i) {
        const unsigned key = candidates[i].layer * 2u + candidates[i].priority;
        if (best < 0 || key >= best_key) {
            best_key = key;
            best = static_cast<int>(i);
        }
    }
    return best;
}
//@LABS-END

}  // namespace snesbus
