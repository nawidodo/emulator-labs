#pragma once
#include <cstdint>

// 90_debug — a copy of the Chapter 21 background pixel path with ONE seeded
// defect. The stub (skeleton) side carries the bug; the solution side is
// correct. See DEBUGGING.md for the observed symptoms.
namespace nes21dbg {

// Same quadrant rule as nes21bg::attribute_bits — or is it?
//
//@LABS-BEGIN 1
//@LABS-SOLUTION
inline int attribute_bits(uint8_t at_byte, int coarse_x, int coarse_y) {
    int shift = ((coarse_y & 2) << 1) | (coarse_x & 2);
    return (at_byte >> shift) & 3;
}
//@LABS-STUB
// Seeded defect: do NOT "fix" this by rewriting the function from scratch —
// find the one wrong token. Symptom report in DEBUGGING.md.
inline int attribute_bits(uint8_t at_byte, int coarse_x, int coarse_y) {
    int shift = ((coarse_x & 2) << 1) | (coarse_y & 2);
    return (at_byte >> shift) & 3;
}
//@LABS-END

// Transparent-safe palette lookup used by the debug renderer: tile color 0
// always shows the universal backdrop ($3F00), never the bg palettes.
inline int palette_entry(const uint8_t* pal, int attr, int color) {
    if (color == 0) return pal[0x00];
    return pal[(attr << 2) | color];
}

// Render one 8x8 tile's pixels through `out_fn(px, py, palette_index)`.
template <typename Fn>
void render_tile(const uint8_t* chr, uint16_t pattern_base, uint8_t tile,
                 int attr, int screen_x, int screen_y, Fn out_fn,
                 const uint8_t* pal) {
    for (int fy = 0; fy < 8; ++fy) {
        uint8_t low = chr[pattern_base + tile * 16 + fy];
        uint8_t high = chr[pattern_base + tile * 16 + 8 + fy];
        for (int fx = 0; fx < 8; ++fx) {
            int bit = 7 - fx;
            int color = ((low >> bit) & 1) | (((high >> bit) & 1) << 1);
            out_fn(screen_x + fx, screen_y + fy,
                   palette_entry(pal, attr, color));
        }
    }
}

}  // namespace nes21dbg
