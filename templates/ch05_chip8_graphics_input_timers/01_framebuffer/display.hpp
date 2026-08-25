#pragma once
#include <cstdint>

namespace chip8 {

inline constexpr int kWidth = 64;
inline constexpr int kHeight = 32;

// The CHIP-8 display is a monochrome 64x32 framebuffer. We store it as a
// flat bool array: pixels[y * kWidth + x]. Flat arrays (not vector<vector>)
// keep the hot DXYN path cache-friendly and match how real emulator
// backends blit linear buffers.
struct Display {
    bool pixels[kWidth * kHeight] = {};

//@LABS-BEGIN 1
//@LABS-SOLUTION
    void clear() {
        for (int i = 0; i < kWidth * kHeight; ++i) pixels[i] = false;
    }
//@LABS-STUB
    void clear() {
        // TODO(1): reset every pixel to "off" (false).
        // Stub compiles so the suite runs RED until you finish it.
    }
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
    // Out-of-bounds reads are defined to return false ("no pixel there").
    // Callers (sprite drawing) then never need a separate bounds check.
    bool get(int x, int y) const {
        if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) return false;
        return pixels[y * kWidth + x];
    }
//@LABS-STUB
    // TODO(2): return the pixel state; out-of-bounds coordinates return false.
    bool get(int /*x*/, int /*y*/) const {
        return true;  // wrong on purpose
    }
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
    // Bounds API: out-of-bounds writes are silently ignored. The display is
    // a fixed-size device, not a growable container — clamping or throwing
    // would hide caller bugs, so we drop and let tests catch misuse.
    void set(int x, int y, bool v) {
        if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) return;
        pixels[y * kWidth + x] = v;
    }
//@LABS-STUB
    // TODO(3): write the pixel; ignore writes that fall outside the screen.
    void set(int /*x*/, int /*y*/, bool /*v*/) {}
//@LABS-END
};

}  // namespace chip8
