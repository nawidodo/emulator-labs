#pragma once
#include <cstdint>
#include <cstddef>

#include "hardware.hpp"

// Exercise 5 — VRAM -> RGBA8888 rendering and the FNV-1a 64 frame hash.
//
// VRAM is COLUMN-major: byte (col*32 + y/8) holds the 8 pixels of column
// `col` between rows y&~7 and y|7; bit y%8 is pixel (col, y). Rendering
// to the upright 224x256 image means x = col, y = row — the cabinet
// monitor's rotation lives entirely in this decode. A decoder that mixes
// up the byte index's two factors produces a transposed image (the
// classic Space Invaders emulator bug).

namespace si {

struct Frame {
    static constexpr size_t kBytes =
        size_t(kScreenWidth) * kScreenHeight * 4;
    uint8_t rgba[kBytes] = {};
};

// FNV-1a 64 over raw bytes — the golden-frame digest format.
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

// Byte holding pixel (col, y).
inline uint8_t vram_byte(const uint8_t* vram, int col, int y) {
//@LABS-BEGIN 1
//@LABS-SOLUTION
    // Column-major: 32 bytes per column, 8 vertical pixels per byte.
    return vram[col * 32 + (y >> 3)];
//@LABS-STUB
    // TODO(1): return the VRAM byte that holds pixel (col, y). Remember
    // the layout is COLUMN-major: 32 bytes per column, 8 rows per byte.
    (void)vram;
    (void)col;
    (void)y;
    return 0x00;  // wrong on purpose: screen renders blank
//@LABS-END
}

// True when pixel (col, y) is lit.
inline bool vram_bit(const uint8_t* vram, int col, int y) {
//@LABS-BEGIN 2
//@LABS-SOLUTION
    return ((vram_byte(vram, col, y) >> (y & 7)) & 1) != 0;
//@LABS-STUB
    // TODO(2): test bit (y % 8) of the byte from TODO(1).
    (void)vram;
    (void)col;
    (void)y;
    return false;  // wrong on purpose
//@LABS-END
}

// Decode all 7 KiB of VRAM into an upright 224x256 RGBA8888 frame.
inline void render_frame(const uint8_t* vram, Frame* out) {
//@LABS-BEGIN 3
//@LABS-SOLUTION
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
//@LABS-STUB
    // TODO(3): fill `out` for every pixel (col, y): lit pixels get the FG
    // color, dark ones the BG color, alpha always 0xFF. Output row stride
    // is kScreenWidth pixels (RGBA, 4 bytes each).
    (void)vram;
    (void)out;
//@LABS-END
}

}  // namespace si
