#pragma once
#include <cstdint>
#include "bus_tables.hpp"
#include "region.hpp"
#include "timing.hpp"

namespace coding {

// CODING TEST: support for an unseen FLASHCART chip, specified only by
// the datasheet lines in CODING_TEST.md. Implement routing, mirroring
// and burst timing exactly as written there.

constexpr unsigned kFlashN = 8;   // non-sequential cycles
constexpr unsigned kFlashS = 3;   // sequential cycles

//@LABS-BEGIN 1
//@LABS-SOLUTION
// FLASHCART occupies the 0x0F top-byte window.
inline bool flash_present(uint32_t addr) {
    return (addr >> 24) == 0x0F;
}
//@LABS-STUB
inline bool flash_present(uint32_t addr) {
    // TODO(1): true iff addr is inside the 0x0F window.
    (void)addr;
    return false;
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Canonical offset: mirrors every 64 KB.
inline uint32_t flash_offset(uint32_t addr) {
    return addr & 0xFFFFu;
}
//@LABS-STUB
inline uint32_t flash_offset(uint32_t addr) {
    // TODO(2): fold into one 64 K mirror page.
    (void)addr;
    return addr;
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// Burst of `count` adjacent halfword accesses from offset 0. The first
// access follows a refill -> N; the rest are sequential while adjacency
// holds (offset i*2).
inline uint64_t burst_total(unsigned count) {
    if (count == 0) return 0;
    return kFlashN + static_cast<uint64_t>(count - 1) * kFlashS;
}
//@LABS-STUB
inline uint64_t burst_total(unsigned count) {
    // TODO(3): N for the first access, S for each adjacent successor.
    (void)count;
    return 0;
}
//@LABS-END

}  // namespace coding
