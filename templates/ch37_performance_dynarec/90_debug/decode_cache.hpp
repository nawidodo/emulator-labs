#pragma once
// Debugging exercise — a decode cache that never learns about stores.
//
// Everything here looks healthy: the cache hits, the interpreter matches
// the switch reference on ordinary programs, benchmarks run faster. Only
// self-modifying code betrays it. Find the bug, fix it, write
// bug-report.md.
#include "rx8.hpp"

#include <cstdint>
#include <unordered_map>

namespace rx8 {

class DecodeCache {
public:
    const Decoded* lookup(uint32_t pc) {
        auto it = map_.find(pc);
        return it == map_.end() ? nullptr : &it->second;
    }
    void insert(uint32_t pc, const Decoded& d) { map_[pc] = d; }

    // Drop every entry whose instruction word overlaps [addr, addr+len).
    size_t invalidate_range(uint32_t addr, uint32_t len) {
        size_t dropped = 0;
        for (auto it = map_.begin(); it != map_.end();) {
            const uint32_t pc = it->first;
            if (pc < addr + len && addr < pc + 4) {
                it = map_.erase(it);
                ++dropped;
            } else {
                ++it;
            }
        }
        return dropped;
    }

    size_t size() const { return map_.size(); }

private:
    std::unordered_map<uint32_t, Decoded> map_;
};

struct CacheStats {
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t invalidations = 0;
};

class CachedCpu {
public:
    Machine m;
    DecodeCache cache;
    CacheStats stats;

    int step();
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline int CachedCpu::step() {
    if (m.halted || m.fault) return 0;
    const uint32_t at = m.pc;
    m.pc += 4;

    Decoded d;
    if (const Decoded* hit = cache.lookup(at)) {
        d = *hit;
        ++stats.hits;
    } else {
        d = decode(m.load_word(at));
        if (m.fault) return 0;
        cache.insert(at, d);
        ++stats.misses;
    }

    rx8::execute(m, d);

    // A store that landed inside decoded code killed those bytes: drop the
    // matching entries BEFORE they can be served again. SW cannot change
    // its own base register, so recomputing the effective address after
    // execute is safe (rd is the BASE register for SW).
    if (d.op == OP_SW && !m.fault) {
        stats.invalidations +=
            cache.invalidate_range(m.r[d.rd] + uint32_t(d.simm()), 4);
    }
    ++m.executed;
    return 1;
}
//@LABS-STUB
// TODO(1): review this cached dispatch loop end-to-end against the SMC
// fixture before calling it done (see DEBUGGING.md for the symptom).
inline int CachedCpu::step() {
    if (m.halted || m.fault) return 0;
    const uint32_t at = m.pc;
    m.pc += 4;

    Decoded d;
    if (const Decoded* hit = cache.lookup(at)) {
        d = *hit;
        ++stats.hits;
    } else {
        d = decode(m.load_word(at));
        if (m.fault) return 0;
        cache.insert(at, d);
        ++stats.misses;
    }

    rx8::execute(m, d);

    // NOTE: no explicit cache maintenance is needed here. Programs in this
    // lab are loaded once at address 0 and treated as immutable, so cached
    // decodes stay valid forever.
    ++m.executed;
    return 1;
}
//@LABS-END

}  // namespace rx8
