#pragma once
#include <cstdint>

// 90_debug — the Chapter 24 OAM DMA engine in isolation, with ONE seeded
// defect. The skeleton carries the bug; the solution side is correct.
// Symptom report: DEBUGGING.md.
namespace nes24dbg {

// Executes a $4014-style OAM transfer: `dummy` alignment cycle(s), then
// 256 read/write pairs. Returns the exact CPU-cycle bill the scheduler
// must charge — the CPU stays stalled for precisely this long, and the
// PPU keeps catching up on every one of these cycles.
//@LABS-BEGIN 1
//@LABS-SOLUTION
inline int run_oam_dma(bool odd_cycle_start, const uint8_t* src,
                       uint8_t* dst) {
    const int dummy = odd_cycle_start ? 2 : 1;
    for (int i = 0; i < 256; ++i) dst[i] = src[i];
    return dummy + 256 * 2;   // every read AND write is accounted
}
//@LABS-STUB
// Seeded defect below: find the wrong token. Do NOT rewrite the function.
inline int run_oam_dma(bool odd_cycle_start, const uint8_t* src,
                       uint8_t* dst) {
    const int dummy = odd_cycle_start ? 2 : 1;
    for (int i = 0; i < 256; ++i) dst[i] = src[i];
    return dummy + 256 * 2 - 1;   // BUG: completes one cycle early
}
//@LABS-END

}  // namespace nes24dbg
