// palette.hpp — DMG background palette mapping and shade-to-RGBA output.
//
// BGP maps each 2bpp tile color index to a *shade* 0..3. On DMG the shades
// run lightest-to-darkest: 0 = white, 3 = black. The emulator's framebuffer
// is RGBA8; we use a fixed grayscale ramp so golden frame hashes are
// platform independent.
#pragma once

#include <cstdint>

namespace gbpal {

struct Rgba {
    uint8_t r, g, b, a;
};

// Translate a tile color index through the two-bit field of BGP selected
// by that index: shade = (bgp >> (index*2)) & 3.
inline uint8_t applyBGP(uint8_t index, uint8_t bgp) {
//@LABS-BEGIN 1
//@LABS-SOLUTION
    return static_cast<uint8_t>((bgp >> (index * 2)) & 3);
//@LABS-STUB
    // TODO(1): select the 2-bit field of BGP for this index.
    (void)index;
    (void)bgp;
    return 0;
//@LABS-END
}

// Fixed grayscale ramp shared by every chapter-14 renderer:
//   shade 0 -> (255,255,255) white ... shade 3 -> (0,0,0) black,
//   alpha always 255 (opaque).
inline Rgba shadeToRgba(uint8_t shade) {
//@LABS-BEGIN 2
//@LABS-SOLUTION
    static const uint8_t kGray[4] = {255, 192, 96, 0};
    const uint8_t v = kGray[shade & 3];
    return {v, v, v, 255};
//@LABS-STUB
    // TODO(2): map shade through the {255, 192, 96, 0} grayscale ramp.
    (void)shade;
    return {255, 255, 255, 255};
//@LABS-END
}

}  // namespace gbpal
