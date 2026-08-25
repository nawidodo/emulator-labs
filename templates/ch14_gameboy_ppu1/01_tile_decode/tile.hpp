// tile.hpp — Game Boy tile decoding (2bpp, two-plane, interleaved rows).
//
// A tile is 16 bytes: 8 rows of 2 bytes each. Byte pair (2*y, 2*y+1) holds
// row y; byte 2*y is plane 0 (low bit of the color index) and byte 2*y+1 is
// plane 1 (high bit). Within a byte, bit 7 is pixel x=0 — the hardware
// shifts rows out MSB first, so the bit order is reversed vs. memory order.
#pragma once

#include <cstdint>

namespace gbtiles {

constexpr int kTileW = 8;
constexpr int kTileH = 8;
constexpr int kTileBytes = 16;

// Extract one pixel's bit from a plane byte. Pixel x=0 is the MSB.
inline uint8_t planeBit(uint8_t planeByte, int x) {
//@LABS-BEGIN 1
//@LABS-SOLUTION
    return static_cast<uint8_t>((planeByte >> (7 - x)) & 1);
//@LABS-STUB
    // TODO(1): return bit (7 - x) of planeByte; all other bits zero.
    (void)planeByte;
    (void)x;
    return 0;
//@LABS-END
}

// Combine the two planes of one pixel into its 2-bit color index:
//   index = low_plane_bit | (high_plane_bit << 1)
inline uint8_t tilePixel(const uint8_t* tile, int x, int y) {
//@LABS-BEGIN 2
//@LABS-SOLUTION
    const uint8_t lo = tile[2 * y];
    const uint8_t hi = tile[2 * y + 1];
    return static_cast<uint8_t>(planeBit(lo, x) | (planeBit(hi, x) << 1));
//@LABS-STUB
    // TODO(2): read row y's two plane bytes and combine their bits.
    (void)tile;
    (void)x;
    (void)y;
    return 0;
//@LABS-END
}

// Decode a full tile into 64 row-major color indices (out[y * 8 + x]).
inline void decodeTile(const uint8_t* tile, uint8_t out[64]) {
//@LABS-BEGIN 3
//@LABS-SOLUTION
    for (int y = 0; y < kTileH; ++y)
        for (int x = 0; x < kTileW; ++x)
            out[y * kTileW + x] = tilePixel(tile, x, y);
//@LABS-STUB
    // TODO(3): fill out with tilePixel for every (x, y).
    (void)tile;
    (void)out;
//@LABS-END
}

}  // namespace gbtiles
