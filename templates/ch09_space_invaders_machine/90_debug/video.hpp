#pragma once
#include <cstdint>
#include <cstddef>

// Debugging exercise — the VRAM renderer with a SEEDED BUG.
//
// Same palette and output format as exercise 5, but the decode is wrong
// in one structural way. The image it produces is not merely shifted —
// it is SCRAMBLED in a very specific, symmetric way. See DEBUGGING.md.

namespace si {

constexpr int kScreenWidth   = 224;
constexpr int kScreenHeight  = 256;
constexpr size_t kVramSize   = 0x1C00;

struct Frame {
    static constexpr size_t kBytes =
        size_t(kScreenWidth) * kScreenHeight * 4;
    uint8_t rgba[kBytes] = {};
};

inline uint64_t fnv64(const uint8_t* data, size_t n) {
    uint64_t h = 0xCBF29CE484222325ull;
    for (size_t i = 0; i < n; ++i) {
        h ^= data[i];
        h *= 0x100000001B3ull;
    }
    return h;
}

constexpr uint8_t kFgR = 0xFF, kFgG = 0xFF, kFgB = 0xFF;
constexpr uint8_t kBgR = 0x00, kBgG = 0x00, kBgB = 0x00;

inline bool vram_bit(const uint8_t* vram, int col, int y) {
//@LABS-BEGIN 2
//@LABS-STUB
    // BUG(2): the two factors of the byte index are swapped — this reads
    // the framebuffer as if it were row-major instead of column-major.
    // TODO(2): decode the byte address column-major again.
    return ((vram[(y >> 3) * 32 + col] >> (y & 7)) & 1) != 0;
//@LABS-SOLUTION
    // Column-major: byte (col*32 + y/8), pixel bit y%8.
    return ((vram[col * 32 + (y >> 3)] >> (y & 7)) & 1) != 0;
//@LABS-END
}

inline void render_frame(const uint8_t* vram, Frame* out) {
    for (int col = 0; col < kScreenWidth; ++col) {
        for (int y = 0; y < kScreenHeight; ++y) {
            const bool on = vram_bit(vram, col, y);
            uint8_t* px =
                &out->rgba[(size_t(y) * kScreenWidth + col) * 4];
            px[0] = on ? kFgR : kBgR;
            px[1] = on ? kFgG : kBgG;
            px[2] = on ? kFgB : kBgB;
            px[3] = 0xFF;
        }
    }
}

}  // namespace si
