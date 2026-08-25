#pragma once
#include <cstdint>
#include <vector>
#include "bus_tables.hpp"
#include "region.hpp"

namespace gba {

// Wait-state accounting. An access is SEQUENTIAL when it directly follows
// an access of the same width to the adjacent address in the same region.
// After a pipeline refill (branch, exception, anything that breaks linear
// fetch) the next access is always NON-sequential — with prefetch OFF
// nothing hides that penalty.

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Sequential iff same region and cur == prev + size. `refill` (a branch
// just happened) forces non-sequential regardless of adjacency.
inline bool is_sequential(uint32_t prev_addr, uint32_t addr,
                          unsigned size_bytes, bool refill) {
    if (refill) return false;
    return route(addr) == route(prev_addr) && addr == prev_addr + size_bytes;
}
//@LABS-STUB
inline bool is_sequential(uint32_t prev_addr, uint32_t addr,
                          unsigned size_bytes, bool refill) {
    // TODO(1): sequential = same region AND adjacent address AND no
    // pending pipeline refill. A refill forces non-sequential even when
    // the address happens to be adjacent.
    (void)prev_addr; (void)addr; (void)size_bytes; (void)refill;
    return true;
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Cost of one access in its region given the sequential flag.
inline unsigned access_cycles(Region r, bool seq) {
    const AccessCost c = cost_of(r);
    return seq ? c.s : c.n;
}
//@LABS-STUB
inline unsigned access_cycles(Region r, bool seq) {
    // TODO(2): pick the S or N cost from the region table.
    (void)r; (void)seq;
    return 1;
}
//@LABS-END

struct SeqStep {
    uint32_t prev;   // previous access address
    uint32_t addr;   // this access address
    unsigned width;  // bytes
};

// Total cycles for a sequence of accesses starting after a pipeline
// refill: each step's sequential flag comes from is_sequential against
// the previous step's address.
inline uint64_t sequence_cycles(const std::vector<SeqStep>& steps) {
    uint64_t total = 0;
    for (size_t i = 0; i < steps.size(); ++i) {
        const Region r = route(steps[i].addr);
        const bool refill = i == 0;   // sequence starts after a refill
        const bool seq =
            is_sequential(steps[i].prev, steps[i].addr, steps[i].width,
                          refill);
        total += access_cycles(r, seq);
    }
    return total;
}

}  // namespace gba
