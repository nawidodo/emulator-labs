// renderer.hpp — BG rendering helpers under repair in the debugging drill.
//
// THREE defects are seeded below. Each produces a plausible-looking image,
// so eyeballing is not enough — isolate each one with the failing tests
// and document it in bug-report.md:
//   bug / root cause / first divergence / fix / regression test.
#pragma once

#include <cstdint>

namespace gbdbg {

constexpr uint8_t kLcdcBgEnable = 0x01;
constexpr uint8_t kLcdcBgMapHi = 0x08;
constexpr uint8_t kLcdcTileUnsigned = 0x10;

struct PpuState {
    uint8_t vram[0x2000];
    uint8_t lcdc, bgp, scy, scx;
};

// Translate a tile color index through BGP into a shade.
// Symptom of defect 1: whole frames look like a photographic negative
// whenever BGP is not the default identity mapping.
inline uint8_t applyBGP(uint8_t index, uint8_t bgp) {
//@LABS-BEGIN 1
//@LABS-STUB
    // BUG(1): reads the palette fields in reverse order.
    return static_cast<uint8_t>((bgp >> ((3 - index) * 2)) & 3);
//@LABS-SOLUTION
    return static_cast<uint8_t>((bgp >> (index * 2)) & 3);
//@LABS-END
}

// Tilemap entry at TILE coordinates (tx, ty). Coordinates come straight
// from scrolled surface math and may exceed 31 or be negative before the
// hardware's implicit wrap.
// Symptom of defect 2: garbage columns/rows appear once scroll pushes tile
// indices past 31 (reads past the end of the 1 KB map).
inline uint8_t mapEntry(const PpuState& s, int tx, int ty) {
//@LABS-BEGIN 2
//@LABS-STUB
    // BUG(2): wraps neither coordinate.
    const uint16_t base = (s.lcdc & kLcdcBgMapHi) ? 0x1C00 : 0x1800;
    return s.vram[base + ty * 32 + tx];
//@LABS-SOLUTION
    const uint16_t base = (s.lcdc & kLcdcBgMapHi) ? 0x1C00 : 0x1800;
    return s.vram[base + (ty & 31) * 32 + (tx & 31)];
//@LABS-END
}

// VRAM offset of a tile's data given its map byte and LCDC bit4.
//   bit4=1 ($8000 mode): unsigned index * 16
//   bit4=0 ($8800 mode): SIGNED index relative to $1000
// Symptom of defect 3: games using $8800 addressing draw tiles from the
// far end of VRAM (map byte $FF should be tile -1, not tile +255).
inline uint16_t tileDataOffset(uint8_t lcdc, uint8_t index) {
//@LABS-BEGIN 3
//@LABS-STUB
    // BUG(3): ignores the signed addressing mode entirely.
    return static_cast<uint16_t>(index * 16);
//@LABS-SOLUTION
    if (lcdc & kLcdcTileUnsigned)
        return static_cast<uint16_t>(index * 16);
    return static_cast<uint16_t>(
        0x1000 + static_cast<int>(static_cast<int8_t>(index)) * 16);
//@LABS-END
}

}  // namespace gbdbg
