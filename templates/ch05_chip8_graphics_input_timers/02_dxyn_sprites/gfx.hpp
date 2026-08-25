#pragma once
#include <cstdint>

namespace chip8 {

inline constexpr int kWidth = 64;
inline constexpr int kHeight = 32;

// The CHIP-8 display: monochrome 64x32, flat row-major bool array.
// (Built step by step in 01_framebuffer; repeated here so each exercise is
// self-contained.)
struct Display {
    bool pixels[kWidth * kHeight] = {};

    void clear() {
        for (int i = 0; i < kWidth * kHeight; ++i) pixels[i] = false;
    }
    // Bounds API: OOB reads are false, OOB writes are dropped.
    bool get(int x, int y) const {
        if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) return false;
        return pixels[y * kWidth + x];
    }
    void set(int x, int y, bool v) {
        if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) return;
        pixels[y * kWidth + x] = v;
    }
};

// Quirk switches where course-accurate behaviour is one of two documented
// real-hardware behaviours. Chapter 6 turns more of these on.
struct Chip8Quirks {
    // false (default): DXYN clips — pixels outside the screen are dropped,
    //                  the rest of the sprite still draws.
    // true           : DXYN wraps — pixel coordinates are taken modulo the
    //                  screen size, so sprites re-enter the opposite edge.
    bool wrapping = false;
};

// Pixel coordinate with wrap/clip policy applied.
// Returns false when the pixel must be skipped (clip mode, off-screen).
inline bool locate_pixel(int x, int y, const Chip8Quirks& quirks,
                         int* out_x, int* out_y) {
//@LABS-BEGIN 1
//@LABS-SOLUTION
    if (quirks.wrapping) {
        // Always wrap into [0, dim): also correct for x >= 64, not just
        // small overshoot at the edges.
        *out_x = ((x % kWidth) + kWidth) % kWidth;
        *out_y = ((y % kHeight) + kHeight) % kHeight;
        return true;
    }
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) return false;
    *out_x = x;
    *out_y = y;
    return true;
//@LABS-STUB
    // TODO(1): apply the quirk's clip/wrap policy to one pixel coordinate.
    // - wrapping: take x mod kWidth and y mod kHeight (result in range)
    // - clipping: return false when the pixel lands off-screen
    (void)x; (void)y; (void)quirks; (void)out_x; (void)out_y;
    return false;  // wrong on purpose
//@LABS-END
}

// Draws an 8-pixel-wide, row_count-tall XOR sprite with its top-left corner
// at (origin_x, origin_y). `rows` points to row_count bytes; bit 7 of a byte
// is the leftmost pixel of that row.
//
// Returns true if any LIT screen pixel was erased (the value stored in VF).
// Erasing is a collision; lighting empty pixels never is. This asymmetry is
// what CHIP-8 games use for hit detection.
inline bool draw_sprite(Display& d, const uint8_t* rows, int row_count,
                        int origin_x, int origin_y,
                        const Chip8Quirks& quirks) {
//@LABS-BEGIN 2
//@LABS-SOLUTION
    bool collision = false;
    for (int row = 0; row < row_count; ++row) {
        const uint8_t bits = rows[row];
        for (int bit = 0; bit < 8; ++bit) {
            if (!((bits >> (7 - bit)) & 1)) continue;
            int px = origin_x + bit;
            int py = origin_y + row;
            if (!locate_pixel(px, py, quirks, &px, &py)) continue;
            const bool before = d.get(px, py);
            d.set(px, py, !before);          // XOR
            collision = collision || before; // lit -> erased counts
        }
    }
    return collision;
//@LABS-STUB
    // TODO(2): XOR every set sprite bit onto the display and report collision.
    // - skip sprite bits that are 0
    // - use locate_pixel() so clip/wrap stays in one place
    // - collision == true iff any already-lit pixel was turned OFF
    (void)d; (void)rows; (void)row_count; (void)origin_x; (void)origin_y;
    (void)quirks;
    return true;  // wrong on purpose
//@LABS-END
}

}  // namespace chip8
