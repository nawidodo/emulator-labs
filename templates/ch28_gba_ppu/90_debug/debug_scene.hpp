#pragma once
// Debugging exercise: sprite-vs-background priority resolution.
//
// The STUB side of the block below contains SEEDED BUGS; the SOLUTION side
// is correct hardware behavior. See DEBUGGING.md for observed symptoms.
#include <cstdint>

namespace gba {

enum class Winner { kBackdrop, kBackground, kSprite };

//@LABS-BEGIN 1
//@LABS-SOLUTION
// GBA rule: the LOWER priority value wins; an opaque sprite beats any
// background at EQUAL priority; transparent pixels never occlude.
inline Winner resolve_top(bool bg_opaque, int bg_prio, bool obj_opaque,
                          int obj_prio) {
    if (obj_opaque && (!bg_opaque || obj_prio <= bg_prio)) return Winner::kSprite;
    if (bg_opaque) return Winner::kBackground;
    return Winner::kBackdrop;
}
//@LABS-STUB
// TODO(1): this comparator decides which layer is drawn on top. One
// relational operator here is inverted relative to real hardware, which
// flips who wins in whole classes of scenes. Find it.
inline Winner resolve_top(bool bg_opaque, int bg_prio, bool obj_opaque,
                          int obj_prio) {
    if (obj_opaque && (!bg_opaque || obj_prio > bg_prio)) return Winner::kSprite;
    if (bg_opaque) return Winner::kBackground;
    return Winner::kBackdrop;
}
//@LABS-END

}  // namespace gba
