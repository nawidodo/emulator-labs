#pragma once
// Textual VRAM / framebuffer viewers. GB-style tiles are 16 bytes:
// 8 rows of two bytes (low plane, high plane); each pixel pair is 2bpp
// (0..3) rendered as " .+#". CHIP-8 framebuffers are 64x32 mono.
#include <cstdint>
#include <string>
#include <vector>

namespace view {

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Decode one tile row: lo/hi plane bytes, bit 7 = leftmost pixel.
inline std::string decode_row(uint8_t lo, uint8_t hi) {
    static const char kShade[4] = {' ', '.', '+', '#'};
    std::string row;
    for (int bit = 7; bit >= 0; --bit) {
        const int color =
            (((hi >> bit) & 1) << 1) | ((lo >> bit) & 1);
        row += kShade[color];
    }
    return row;
}
//@LABS-STUB
// TODO(1): combine the two bit planes into 2bpp pixels, MSB first,
// rendered with the shades " .+#" (0..3).
inline std::string decode_row(uint8_t, uint8_t) { return "        "; }
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Render a full 16-byte GB-style tile as 8 lines.
inline std::vector<std::string> render_tile(
    const uint8_t* vram,      // 16 bytes
    uint16_t tile_index) {
    const uint8_t* t = vram + size_t(tile_index) * 16;
    std::vector<std::string> lines;
    for (int row = 0; row < 8; ++row)
        lines.push_back(decode_row(t[row * 2], t[row * 2 + 1]));
    return lines;
}
//@LABS-STUB
inline std::vector<std::string> render_tile(const uint8_t*, uint16_t) {
    // TODO(2): rows come in lo,hi pairs; use decode_row per row.
    return {};
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// Render a CHIP-8 64x32 mono framebuffer as text ('#' set, '.' clear),
// one string per scanline.
inline std::vector<std::string> render_chip8_fb(const uint8_t* fb) {
    std::vector<std::string> lines;
    for (int y = 0; y < 32; ++y) {
        std::string line;
        for (int x = 0; x < 64; ++x)
            line += fb[size_t(y) * 64 + size_t(x)] ? '#' : '.';
        lines.push_back(line);
    }
    return lines;
}
//@LABS-STUB
inline std::vector<std::string> render_chip8_fb(const uint8_t*) {
    // TODO(3): 32 scanlines of 64 chars each; set pixels '#', clear '.'.
    return {};
}
//@LABS-END

}  // namespace view
