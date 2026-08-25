#pragma once
#include <cstdint>
#include <cstddef>

#include "hardware.hpp"

// VRAM -> RGBA8888 renderer and the FNV-1a 64 frame hasher.
//
// VRAM is COLUMN-major: byte (col*32 + y/8), pixel bit y%8. Rendering to
// the upright 224x256 image means x = col, y = row — the framebuffer
// rotation happens in this decode, nowhere else.

namespace si {

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

// Foreground white / background black; alpha opaque on both so the dump
// is a well-formed RGBA8888 image.
constexpr uint8_t kFgR = 0xFF, kFgG = 0xFF, kFgB = 0xFF;
constexpr uint8_t kBgR = 0x00, kBgG = 0x00, kBgB = 0x00;

}  // namespace si

namespace si {

// Byte holding pixel (col, y) and the bit test for that pixel.
inline uint8_t vram_byte(const uint8_t* vram, int col, int y) {
    return vram[col * 32 + (y >> 3)];
}

inline bool vram_bit(const uint8_t* vram, int col, int y) {
    return ((vram_byte(vram, col, y) >> (y & 7)) & 1) != 0;
}

// Decode the whole 7 KiB of column-major VRAM into an upright
// 224x256 RGBA8888 frame. Every byte of VRAM is touched exactly once;
// output pixels are written exactly once.
inline void render_frame(const uint8_t* vram, Frame* out) {
    for (int col = 0; col < kScreenWidth; ++col) {
        for (int y = 0; y < kScreenHeight; ++y) {
            const bool on = vram_bit(vram, col, y);
            uint8_t* px = &out->rgba[(size_t(y) * kScreenWidth + col) * 4];
            px[0] = on ? kFgR : kBgR;
            px[1] = on ? kFgG : kBgG;
            px[2] = on ? kFgB : kBgB;
            px[3] = 0xFF;
        }
    }
}

}  // namespace si
