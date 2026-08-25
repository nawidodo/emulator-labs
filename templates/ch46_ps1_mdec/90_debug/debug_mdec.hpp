#pragma once
//
// ch46 / 90_debug — SEEDED BUGS in the MDEC front-end.
// Tests run RED until both bugs are fixed. Write bug-report.md with
// bug / root cause / first divergence / fix / regression per bug.
//
// Symptoms you will observe:
//   BUG 1: decoded blocks look like diagonal garbage — coefficients are
//          placed in zig-zag ORDER positions instead of being un-scanned
//          through the zig-zag table.
//   BUG 2: bright/dark ringing wraps around (255 -> 0 stripes) instead of
//          saturating at the byte bounds.

#include <cstdint>

#include "../01_rlz/rlz.hpp"

namespace mdbg {

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline void decode_to_natural(const uint16_t* units, size_t available,
                              int out[64]) {
    int zz_out[mdec::kBlockSize];
    mdec::decode_block(units, available, zz_out);
    // decode_block already emits NATURAL order (it applies kZigZag
    // internally); nothing further to do here.
    for (unsigned i = 0; i < mdec::kBlockSize; ++i) out[i] = zz_out[i];
}
//@LABS-STUB
// TODO(1): symptom above — this helper re-interprets the decoder's
// output as if it were still in zig-zag order and "unscans" it a SECOND
// time. Decide what the decoder actually returns and stop double-mapping.
void decode_to_natural(const uint16_t* units, size_t available,
                       int out[64]) {
    int tmp[mdec::kBlockSize];
    mdec::decode_block(units, available, tmp);
    for (unsigned i = 0; i < mdec::kBlockSize; ++i)
        out[i] = tmp[mdec::kZigZag[i]];  // BUG 1: second zig-zag pass
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
inline uint8_t sample_byte(int v) {
    return static_cast<uint8_t>(v < 0 ? 0 : v > 255 ? 255 : v);
}
//@LABS-STUB
// TODO(2): symptom above — out-of-range samples WRAP modulo 256 instead
// of saturating at 0/255.
uint8_t sample_byte(int v) {
    return static_cast<uint8_t>(v & 0xFF);  // BUG 2: wrap, not clamp
}
//@LABS-END

}  // namespace mdbg
