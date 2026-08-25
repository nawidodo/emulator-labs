#pragma once
//
// ch46 / 03_color_convert — YCbCr -> RGB15 conversion and 16x16
// macroblock assembly (psx-spx "MDEC color output").
//
// Documented arithmetic:
//   r = y + ((1436 * cr) >> 10)
//   g = y - ((352 * cb + 731 * cr) >> 10)
//   b = y + ((1815 * cb) >> 10)
// with round-to-nearest on the >>10 terms ((v + 512) >> 10 for the
// positive subtrahends handled via the same bias on each product sum),
// then clamped to 0..255. RGB15 packs BGR555: word = b3<<10 | g3<<5 | r3.
//
// A macroblock is six decoded 8x8 blocks: Y0 Y1 Y2 Y3 Cb Cr. Chroma is
// upsampled x2 (nearest neighbor) over the 16x16 luma area.

#include <cstdint>

#include "../02_idct/idct.hpp"

namespace mdec {

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline uint8_t clamp_byte(int v) {
    return static_cast<uint8_t>(v < 0 ? 0 : v > 255 ? 255 : v);
}

inline uint16_t ycbcr_to_rgb15(int y, int cb, int cr) {
    const int r = clamp_byte(y + (((1436 * cr) + 512) >> 10));
    const int g =
        clamp_byte(y - (((352 * cb + 731 * cr) + 512) >> 10));
    const int b = clamp_byte(y + (((1815 * cb) + 512) >> 10));
    return static_cast<uint16_t>((b >> 3) << 10 | (g >> 3) << 5 | (r >> 3));
}
//@LABS-STUB
// TODO(1): implement the documented constants with rounding bias +512
// before each >>10, clamping to 0..255, and BGR555 packing.
uint16_t ycbcr_to_rgb15(int y, int cb, int cr) {
    (void)y; (void)cb; (void)cr;
    return 0;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Assembles one macroblock from six IDCTed 8x8 blocks into 256 RGB15
// words (row-major 16x16). Block order: Y0 Y1 Y2 Y3 Cb Cr.
inline void assemble_macroblock(const int blocks[6][64],
                                uint16_t out[256]) {
    const auto* y_blocks = blocks;
    const auto& cb = blocks[4];
    const auto& cr = blocks[5];
    for (int my = 0; my < 16; ++my) {
        for (int mx = 0; mx < 16; ++mx) {
            const int yblk = (my / 8) * 2 + (mx / 8);
            const int y = y_blocks[yblk][(my % 8) * 8 + (mx % 8)];
            // Chroma: nearest-neighbor x2 upscale.
            const int ci = (my / 2) * 8 + (mx / 2);
            out[my * 16 + mx] =
                ycbcr_to_rgb15(y, cb[ci], cr[ci]);
        }
    }
}
//@LABS-STUB
// TODO(2): map each of the 16x16 pixels to its Y block (Y0 top-left,
// Y1 top-right, Y2 bottom-left, Y3 bottom-right), sample Cb/Cr with x2
// nearest-neighbor upscale, convert, store row-major.
void assemble_macroblock(const int blocks[6][64], uint16_t out[256]) {
    (void)blocks; (void)out;
}
//@LABS-END

}  // namespace mdec
