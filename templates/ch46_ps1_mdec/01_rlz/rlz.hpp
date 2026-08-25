#pragma once
//
// ch46 / 01_rlz — MDEC variable-length (RLE/RLZ) coefficient decoder
// (psx-spx "MDEC Decoding").
//
// Compressed block stream (16-bit units, big-endian inside each 32-bit
// DMA word — this lab works directly on 16-bit units):
//   unit 0     : [15] Q table select (0 = luminance, 1 = default/chroma)
//                [14:0] quantizer scale (unsigned)
//   data units : [15:10] run  = number of ZERO coefficients before this
//                            one (in ZIG-ZAG order)
//                [ 9:0] level = signed magnitude value
//   0xFE00     : end-of-block
//
// Dequantization (documented, truncating division toward zero):
//   position 0 (DC): value = level * scale * QTAB[0] / 8
//   position p>0    : value = level * scale * QTAB[p] / 16
//
// Coefficients land in NATURAL order via the standard zig-zag scan.

#include <cstdint>
#include <cstdlib>
#include <vector>

namespace mdec {

constexpr unsigned kBlockSize = 64;
constexpr uint16_t kEndOfBlock = 0xFE00;

struct StreamHeader {
    bool chroma_table;  // true => second (default) Q table
    uint16_t scale;
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline StreamHeader parse_header(uint16_t unit) {
    return {(unit & 0x8000u) != 0,
            static_cast<uint16_t>(unit & 0x7FFFu)};
}
//@LABS-STUB
// TODO(1): bit 15 selects the Q table; bits 14..0 are the scale.
StreamHeader parse_header(uint16_t unit) {
    (void)unit;
    return {false, 0};  // wrong on purpose
}
//@LABS-END

// Standard zig-zag scan order: natural_index = kZigZag[zz_position].
const uint8_t kZigZag[64] = {
    0,  1,  8,  16, 9,  2,  3,  10,
    17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63,
};

// Quantization tables (documented constants for this lab).
const uint16_t kLumaQ[64] = {
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
};
const uint16_t kDefaultQ[64] = {
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
};
// (Both tables are flat 16 in this lab so quantized levels stay exactly
//  representable; real hardware tables differ but the pipeline is
//  identical. Kept as arrays so later labs can vary them.)

inline const uint16_t* q_table(bool chroma) {
    return chroma ? kDefaultQ : kLumaQ;
}

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Decodes one 8x8 block starting at units[pos]. Returns the number of
// units consumed; fills out[64] in NATURAL order (dequantized, zig-zag
// reversed). Truncation toward zero matches psx-spx notes.
inline size_t decode_block(const uint16_t* units, size_t available,
                           int out[kBlockSize]) {
    for (unsigned i = 0; i < kBlockSize; ++i) out[i] = 0;

    const StreamHeader hdr = parse_header(units[0]);
    const uint16_t* qt = q_table(hdr.chroma_table);
    const int scale = hdr.scale;
    size_t pos = 1;
    unsigned zz = 0;  // current zig-zag position

    while (pos < available) {
        const uint16_t u = units[pos++];
        if (u == kEndOfBlock) break;

        const unsigned run = u >> 10;
        const int level = static_cast<int>(u & 0x3FFu);
        const int signed_level =
            (level & 0x200) ? (level - 0x400) : level;
        zz += run;
        if (zz >= kBlockSize) break;  // malformed: refuse to overrun

        const unsigned natural = kZigZag[zz];
        const int q = qt[natural];
        const int num = signed_level * scale * q;
        const int value = zz == 0 ? num / 8 : num / 16;
        out[natural] = value;  // C++11 division truncates toward zero
    }
    return pos;
}
//@LABS-STUB
// TODO(2): walk run/level units, accumulate the zig-zag position, apply
// the documented dequantization (DC /8, AC /16, truncation toward zero)
// and store into out[] at kZigZag[zz]. Stop at kEndOfBlock. Return the
// number of units consumed INCLUDING the header unit.
size_t decode_block(const uint16_t* units, size_t available,
                    int out[kBlockSize]) {
    (void)units; (void)available; (void)out;
    return 0;  // wrong on purpose
}
//@LABS-END

}  // namespace mdec
