#pragma once
#include <cstdint>
#include <vector>
#include "bus_tables.hpp"
#include "region.hpp"
#include "timing.hpp"

namespace gba {

// Challenge: an access-sequence timing calculator. Given only the region
// tables it must reproduce the exact cycle totals the reference bus
// charges — including the post-refill non-sequential penalty.

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Total cost of `count` sequential-width accesses marching through
// `region` starting at `first_addr` (adjacent addresses). The first
// access follows a pipeline refill and is therefore non-sequential.
inline uint64_t burst_total(Region r, uint32_t first_addr, unsigned count,
                            unsigned width) {
    if (count == 0) return 0;
    uint64_t total = access_cycles(r, false);          // post-refill: N
    uint32_t prev = first_addr;
    for (unsigned i = 1; i < count; ++i) {
        const uint32_t addr = first_addr + i * width;
        total += access_cycles(r, is_sequential(prev, addr, width, false));
        prev = addr;
    }
    return total;
}
//@LABS-STUB
inline uint64_t burst_total(Region r, uint32_t first_addr, unsigned count,
                            unsigned width) {
    // TODO(1): sum access_cycles over `count` adjacent accesses; the
    // first one follows a refill so it bills at the N cost.
    (void)r; (void)first_addr; (void)count; (void)width;
    return 0;
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Which ROM waitstate chip completes a burst soonest? Ties break toward
// the lower region number (Ws0 < Ws1 < Ws2).
inline Region fastest_rom_chip(unsigned count, unsigned width) {
    const Region chips[] = {Region::RomWs0, Region::RomWs1,
                            Region::RomWs2};
    Region best = chips[0];
    uint64_t best_cost = burst_total(chips[0], 0x08000000u, count, width);
    for (const Region c : chips) {
        const uint64_t cost = burst_total(c, 0x08000000u, count, width);
        if (cost < best_cost) {
            best = c;
            best_cost = cost;
        }
    }
    return best;
}
//@LABS-STUB
inline Region fastest_rom_chip(unsigned count, unsigned width) {
    // TODO(2): run burst_total over RomWs0/RomWs1/RomWs2 and return the
    // cheapest (ties -> lowest).
    (void)count; (void)width;
    return Region::RomWs0;
}
//@LABS-END

}  // namespace gba
