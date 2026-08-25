#pragma once
// CODING TEST: DMA3 "special" trigger = video capture mode (spec in
// CODING_TEST.md). The stub rejects every line; the solution follows the
// specification exactly.
#pragma once
#include <cstdint>

namespace gba {

using u16 = uint16_t;
using u32 = uint32_t;
using s32 = int32_t;
using u64 = uint64_t;

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Video-capture DMA (DMA3, timing bits == 3) fires on EVERY scanline from
// line 2 (the prefill line, which transfers twice the normal count is NOT
// modeled — one transfer per line here) through line 162 inclusive, even
// when the repeat bit is clear; enable clears after line 162.
inline bool capture_fires_on_line(int line) {
    return line >= 2 && line <= 162;
}

// Units transferred by a capture DMA whose count field holds `count`
// (0 means full 0x10000, like other channels).
inline u64 capture_units(u16 count) {
    return count == 0 ? 0x10000u : u64(count);
}

// After this line the channel has finished and ENABLE must read as cleared.
inline bool capture_finished_after(int line) { return line >= 162; }
//@LABS-STUB
// TODO(1): implement the three predicates per CODING_TEST.md: fire lines
// are exactly 2..162 inclusive; count 0 decodes to the full 0x10000 units;
// the channel finishes after line 162.
inline bool capture_fires_on_line(int line) {
    (void)line;
    return false;  // wrong on purpose
}
inline u64 capture_units(u16 count) {
    (void)count;
    return 0;  // wrong on purpose
}
inline bool capture_finished_after(int line) {
    (void)line;
    return false;  // wrong on purpose
}
//@LABS-END

}  // namespace gba
