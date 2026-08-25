#pragma once
// Exercise 02 — decode caching with precise invalidation.
//
// The classic interpreter speedup: fetch+decode once per pc, then reuse the
// decoded form until the underlying bytes change. The subtle half is
// invalidation: when a store lands inside decoded code, every cache entry
// covering those bytes must die, or self-modifying code silently runs the
// OLD program (see 90_debug for exactly that bug, seeded).
#include "rx8.hpp"

#include <cstdint>
#include <unordered_map>

namespace rx8 {

class DecodeCache {
public:
    const Decoded* lookup(uint32_t pc);
    void insert(uint32_t pc, const Decoded& d);

    // Drop every entry whose instruction word overlaps [addr, addr+len).
    // Returns the number of entries dropped. Partial overlap must count:
    // stores are byte-granular even though instructions are word-aligned.
    size_t invalidate_range(uint32_t addr, uint32_t len);

    size_t size() const { return map_.size(); }

private:
    std::unordered_map<uint32_t, Decoded> map_;
};

struct CacheStats {
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t invalidations = 0;  // total entries ever dropped
};

// Interpreter front end backed by the decode cache. Architectural state
// lives in `m` (a plain Machine); the cache only replaces fetch+decode.
class CachedCpu {
public:
    Machine m;
    DecodeCache cache;
    CacheStats stats;

    // Test hook for the ch37/90 debugging exercise: setting this false
    // simulates a cache that never learns about stores.
    bool auto_invalidate = true;

    // Returns 1 when an instruction retired, 0 when the machine can no
    // longer continue (halted/faulted) — same contract as Machine::step().
    int step();
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline const Decoded* DecodeCache::lookup(uint32_t pc) {
    auto it = map_.find(pc);
    return it == map_.end() ? nullptr : &it->second;
}

inline void DecodeCache::insert(uint32_t pc, const Decoded& d) {
    map_[pc] = d;
}
//@LABS-STUB
// TODO(1): implement the happy path — lookup() returns nullptr on a miss
// and a pointer to the cached Decoded on a hit; insert() stores the decode
// keyed by pc. Stub never caches so every step is a miss (tests RED).
inline const Decoded* DecodeCache::lookup(uint32_t pc) {
    (void)pc;
    return nullptr;  // wrong on purpose: permanent cache miss
}

inline void DecodeCache::insert(uint32_t pc, const Decoded& d) {
    (void)pc;
    (void)d;
    // wrong on purpose: refuses to remember anything
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
inline size_t DecodeCache::invalidate_range(uint32_t addr, uint32_t len) {
    size_t dropped = 0;
    for (auto it = map_.begin(); it != map_.end();) {
        const uint32_t pc = it->first;
        // Entry covers [pc, pc+4); drop on ANY overlap with [addr, addr+len).
        if (pc < addr + len && addr < pc + 4) {
            it = map_.erase(it);
            ++dropped;
        } else {
            ++it;
        }
    }
    return dropped;
}
//@LABS-STUB
// TODO(2): erase every entry whose instruction window [pc, pc+4) overlaps
// the stored range [addr, addr+len) and return how many died. Getting the
// overlap test backwards is THE dynarec bug class — a too-narrow check
// lets stale code run (see 90_debug).
inline size_t DecodeCache::invalidate_range(uint32_t addr, uint32_t len) {
    (void)addr;
    (void)len;
    return 0;  // wrong on purpose: invalidates nothing
}
//@LABS-END

//@LABS-BEGIN 3
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
    if (auto_invalidate && d.op == OP_SW && !m.fault) {
        stats.invalidations +=
            cache.invalidate_range(m.r[d.rd] + uint32_t(d.simm()), 4);
    }
    ++m.executed;
    return 1;
}
//@LABS-STUB
// TODO(3): wire the cached dispatch loop. Advance m.pc by 4, serve the
// instruction from cache.lookup() (counting stats.hits / stats.misses and
// decoding+inserting on a miss),
// run rx8::execute, bump m.executed, and return 1 on retire / 0 when the
// machine stopped. When auto_invalidate is set and the retired
// instruction was OP_SW that did not fault, drop every cached entry
// overlapping the stored word and add the dropped count to
// stats.invalidations.
inline int CachedCpu::step() {
    if (m.halted || m.fault) return 0;
    const uint32_t at = m.pc;
    m.pc += 4;
    const Decoded d = decode(m.load_word(at));
    if (m.fault) return 0;
    rx8::execute(m, d);
    ++m.executed;
    return 1;  // wrong on purpose: bypasses the cache entirely
}
//@LABS-END

}  // namespace rx8
