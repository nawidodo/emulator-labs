#pragma once
// Three-domain clock model (chapter 33, exercise 03).
//
// The SNES runs five-plus clocks off one crystal. We model the three the
// curriculum chapter cares about from a single master counter:
//
//   master        21477272 Hz (the real 21.47727 MHz clock)
//   CPU      = master / 6   (~3.58 MHz)
//   PPU dot  = master / 4   (~5.37 MHz)
//   APU/SPC  = master / 32  (~671 kHz)  <-- SIMPLIFICATION
//
// APU SIMPLIFICATION, stated plainly: the real SPC700 core clock is about
// master/21.47 (1.024 MHz). We divide by a power of two instead so that
// integer accumulators stay exact with no drift and no floating point;
// every domain tick count is reproducible bit-for-bit across hosts. The
// ratio is WRONG by design; the mechanism (one master timebase, per-domain
// integer dividers) is what this chapter teaches. See LECTURE.md.
//
// Frame ordering contract (used by the 91_challenge runner):
//   hdma_init() once at frame start, then for each visible line:
//   HDMA line effects FIRST, then the PPU draws the line. Register writes
//   produced for line N are visible while line N renders.
#include <cstdint>
#include <vector>

namespace snesdma {

inline constexpr uint64_t kMasterHz = 21477272ull;

// Fixed-phase integer divider: accumulates master ticks and emits one tick
// every `divisor` master ticks, keeping the sub-tick remainder so no ticks
// are lost across calls.
struct Divider {
    int divisor = 1;
    uint64_t remainder = 0;  // master ticks carried toward the next tick
    uint64_t ticks = 0;

    //@LABS-BEGIN 1
    //@LABS-SOLUTION
    void advance(uint64_t master_ticks) {
        remainder += master_ticks;
        ticks += remainder / uint64_t(divisor);
        remainder %= uint64_t(divisor);
    }
    //@LABS-STUB
    // TODO(1): add master ticks to the accumulator, emit floor(remainder /
    // divisor) new domain ticks into `ticks`, and keep the leftover in
    // `remainder`. Losing remainders here desyncs every domain over time.
    void advance(uint64_t /*master_ticks*/) {}
    //@LABS-END
};

// Advances all domains under one master-time target. Deterministic:
// run_until(N) then run_until(M<=N) is always identical to run_until(N).
struct Scheduler {
    Divider cpu{6};
    Divider ppu{4};
    Divider apu{32};  // documented simplification, see file header
    uint64_t master = 0;

    //@LABS-BEGIN 2
    //@LABS-SOLUTION
    void run_until(uint64_t target_master) {
        if (target_master <= master) return;
        const uint64_t delta = target_master - master;
        cpu.advance(delta);
        ppu.advance(delta);
        apu.advance(delta);
        master = target_master;
    }
    //@LABS-STUB
    // TODO(2): advance cpu/ppu/apu dividers by (target - master) and only
    // then update `master`. Never move `master` first or you lose ticks.
    void run_until(uint64_t /*target_master*/) {}
    //@LABS-END
};

// Canonical per-line phase order for one frame, shared by tests and the
// challenge runner. Provided (not stubbed): the ORDER is the contract.
inline std::vector<const char*> frame_phase_order(int visible_lines) {
    std::vector<const char*> phases;
    phases.push_back("hdma_init");
    for (int n = 0; n < visible_lines; ++n) {
        phases.push_back("hdma_line");
        phases.push_back("ppu_draw");
    }
    return phases;
}

}  // namespace snesdma
