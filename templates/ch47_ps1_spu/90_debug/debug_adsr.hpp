#pragma once
#include <cstdint>

#include "../04_adsr/adsr.hpp"

namespace spu {

// Debugging target #2: an SPU envelope decays slightly too fast and its
// golden sample dump is off by one step. The update below carries the bug.
//@LABS-BEGIN 2
//@LABS-SOLUTION
// One exponential decay update; returns the new level.
inline int exp_decay_update(int level, uint8_t step) {
    const int delta = std::max(1, (level * step) >> 6);
    return delta > level ? 0 : level - delta;
}
//@LABS-STUB
// TODO(2): this envelope path carries a SEEDED BUG: golden PCM dumps are
// consistently one quantum lower than the reference during decay. Find
// and fix it, then write bug-report.md.
inline int exp_decay_update(int level, uint8_t step) {
    // Seeded bug: do not change anything except what you diagnose here.
    const int delta = std::max(1, ((level + 1) * step) >> 6);
    return delta > level ? 0 : level - delta;
}
//@LABS-END

}  // namespace spu
