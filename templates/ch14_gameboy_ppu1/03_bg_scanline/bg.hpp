// bg.hpp — background scanline rendering: tilemap fetch + scroll wrap.
//
// The BG layer is a 256x256 pixel surface (32x32 tiles) stored in VRAM at
// $9800 or $9C00 (LCDC bit 3). SCX/SCY ($FF43/$FF42) select the 160-pixel
// window into that surface for the current scanline; coordinates wrap
// modulo 256 so scrolling is seamless.
//
// Tile data lives at $8000 (unsigned indices, LCDC bit 4 = 1) or $8800
// (signed indices centered on tile 0x80, LCDC bit 4 = 0).
#pragma once

#include <cstdint>

namespace gbbg {

constexpr int kScreenWidth = 160;
constexpr int kMapTiles = 32;          // 32x32 tiles
constexpr int kMapPixels = 256;        // 32 * 8

// Minimal register set this exercise needs. VRAM is indexed by address
// minus $8000.
struct BgState {
    uint8_t vram[0x2000];
    uint8_t lcdc;
    uint8_t bgp;
    uint8_t scy;
    uint8_t scx;
};

constexpr uint8_t kLcdcBgEnable = 0x01;
constexpr uint8_t kLcdcBgMapHi = 0x08;   // 1 -> map at $9C00
constexpr uint8_t kLcdcTileUnsigned = 0x10;

// Base VRAM offset of the selected tilemap: $1800 (LCDC bit3=0) or
// $1C00 (LCDC bit3=1).
inline uint16_t bgMapBase(uint8_t lcdc) {
//@LABS-BEGIN 1
//@LABS-SOLUTION
    return (lcdc & kLcdcBgMapHi) ? 0x1C00 : 0x1800;
//@LABS-STUB
    // TODO(1): LCDC bit3 selects the tilemap base.
    (void)lcdc;
    return 0x1800;
//@LABS-END
}

// Tilemap entry with wrap-around: mapY/mapX are already divided to tile
// units but may exceed 31 when scrolled; take them modulo 32 and return
// vram[bgMapBase(lcdc) + mapY*32 + mapX].
inline uint8_t mapEntry(const BgState& s, int mapX, int mapY) {
//@LABS-BEGIN 2
//@LABS-SOLUTION
    const uint16_t base = bgMapBase(s.lcdc);
    return s.vram[base + (mapY & 31) * 32 + (mapX & 31)];
//@LABS-STUB
    // TODO(2): wrap both coordinates modulo 32 before indexing.
    (void)s;
    (void)mapX;
    (void)mapY;
    return 0;
//@LABS-END
}

// VRAM offset of a tile's 16 bytes given its map index byte:
//   unsigned mode (LCDC bit4=1): index * 16
//   signed mode   (LCDC bit4=0): 0x1000 + int8_t(index) * 16
inline uint16_t tileDataOffset(uint8_t lcdc, uint8_t index) {
//@LABS-BEGIN 3
//@LABS-SOLUTION
    if (lcdc & kLcdcTileUnsigned)
        return static_cast<uint16_t>(index * 16);
    return static_cast<uint16_t>(
        0x1000 + static_cast<int>(static_cast<int8_t>(index)) * 16);
//@LABS-STUB
    // TODO(3): honor BOTH addressing modes; signed mode wraps below $1000.
    (void)lcdc;
    (void)index;
    return 0;
//@LABS-END
}

// Render one full BG scanline into shades[0..159] (raw color indices;
// BGP translation happens in exercise 02/04).
// For screen column x the surface pixel is ((scx + x) % 256,
// (scy + ly) % 256); pick tile via mapEntry, pixel inside the tile via
// the low 3 bits of each coordinate. BG disabled => shade 0 everywhere.
void renderBgScanline(const BgState& s, int ly, uint8_t shades[160]) {
//@LABS-BEGIN 4
//@LABS-SOLUTION
    const bool enabled = (s.lcdc & kLcdcBgEnable) != 0;
    for (int x = 0; x < kScreenWidth; ++x) {
        if (!enabled) {
            shades[x] = 0;
            continue;
        }
        const int sx = (s.scx + x) & (kMapPixels - 1);
        const int sy = (s.scy + ly) & (kMapPixels - 1);
        const uint8_t indexByte = mapEntry(s, sx / 8, sy / 8);
        const uint16_t tileOff = tileDataOffset(s.lcdc, indexByte);
        const uint8_t* tile = &s.vram[tileOff];
        const int px = sx & 7;
        const int py = sy & 7;
        shades[x] = static_cast<uint8_t>(((tile[2 * py] >> (7 - px)) & 1) |
                                         (((tile[2 * py + 1] >> (7 - px)) &
                                           1) << 1));
    }
//@LABS-STUB
    // TODO(4): loop x = 0..159: fetch tile + in-tile offset, decode the
    // 2bpp row pair exactly like exercise 01, store the color index.
    // BG disabled (LCDC bit0=0) writes shade 0 everywhere.
    (void)s;
    (void)ly;
    for (int x = 0; x < kScreenWidth; ++x) shades[x] = 0;
//@LABS-END
}

}  // namespace gbbg
