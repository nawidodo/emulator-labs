// win.hpp — window layer and complete frame renderer.
//
// The window is a second tilemap drawn over the BG on scanlines where it
// is active. It never scrolls: WX/WY ($FF4B/$FF4A) position its top-left
// corner on screen, and the window content advances through its own
// internal line counter that only increments on lines where the window
// was actually drawn (this is why a window toggled mid-frame does not
// jump back to WY).
//
// Window active on scanline LY iff:
//   LCDC bit 7 (LCD on) && LCDC bit 5 (window enable) && LY >= WY.
// Screen x is covered by the window iff x >= WX - 7 (WX < 7 hides the
// window entirely — hardware quirk).
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

namespace gbwin {

constexpr int kScreenWidth = 160;
constexpr int kScreenHeight = 144;
using Frame = uint8_t[kScreenHeight][kScreenWidth][4];

constexpr uint8_t kLcdcLcdOn = 0x80;
constexpr uint8_t kLcdcWinEnable = 0x20;
constexpr uint8_t kLcdcWinMapHi = 0x40;   // window map $9C00
constexpr uint8_t kLcdcBgMapHi = 0x08;
constexpr uint8_t kLcdcTileUnsigned = 0x10;
constexpr uint8_t kLcdcBgEnable = 0x01;

struct PpuState {
    uint8_t vram[0x2000];
    uint8_t lcdc, bgp, scy, scx, wy, wx;
};

// Window map base VRAM offset: $1C00 when LCDC bit6 set else $1800.
inline uint16_t winMapBase(uint8_t lcdc) {
//@LABS-BEGIN 1
//@LABS-SOLUTION
    return (lcdc & kLcdcWinMapHi) ? 0x1C00 : 0x1800;
//@LABS-STUB
    // TODO(1): LCDC bit6 selects the window map base.
    (void)lcdc;
    return 0x1800;
//@LABS-END
}

// True when the window draws on this scanline (see file comment).
inline bool windowActive(const PpuState& s, int ly) {
//@LABS-BEGIN 2
//@LABS-SOLUTION
    return (s.lcdc & kLcdcLcdOn) != 0 && (s.lcdc & kLcdcWinEnable) != 0 &&
           ly >= s.wy;
//@LABS-STUB
    // TODO(2): LCD on AND window enabled AND ly >= wy.
    (void)s;
    (void)ly;
    return false;
//@LABS-END
}

// Decode one pixel of the tile at `tileOff` (two-plane, like ex 01).
inline uint8_t tileIndexAt(const PpuState& s, uint16_t tileOff, int px,
                           int py) {
//@LABS-BEGIN 3
//@LABS-SOLUTION
    const uint8_t lo = s.vram[tileOff + 2 * py];
    const uint8_t hi = s.vram[tileOff + 2 * py + 1];
    return static_cast<uint8_t>(((lo >> (7 - px)) & 1) |
                                (((hi >> (7 - px)) & 1) << 1));
//@LABS-STUB
    // TODO(3): two-plane decode exactly like exercise 01.
    (void)s;
    (void)tileOff;
    (void)px;
    (void)py;
    return 0;
//@LABS-END
}

// VRAM offset of a tile's data: unsigned $8000 / signed $8800 addressing.
inline uint16_t tileDataOffset(uint8_t lcdc, uint8_t index) {
//@LABS-BEGIN 4
//@LABS-SOLUTION
    if (lcdc & kLcdcTileUnsigned)
        return static_cast<uint16_t>(index * 16);
    return static_cast<uint16_t>(
        0x1000 + static_cast<int>(static_cast<int8_t>(index)) * 16);
//@LABS-STUB
    // TODO(4): unsigned $8000 / signed $8800 addressing.
    (void)lcdc;
    (void)index;
    return 0;
//@LABS-END
}

// Render one scanline of BG + window into rgba[160][4].
// `windowLine` is the window's internal content line (only meaningful if
// the window is active); BG pixels use SCX/SCY exactly like exercise 04.
// For x >= WX-7 the window pixel replaces the BG pixel.
void renderScanline(const PpuState& s, int ly, int windowLine,
                    uint8_t rgba[160][4]) {
//@LABS-BEGIN 5
//@LABS-SOLUTION
    static const uint8_t kGray[4] = {255, 192, 96, 0};
    const bool lcdOn = (s.lcdc & kLcdcLcdOn) != 0;

    // Background pass (scrolling surface), shade 0 when disabled.
    for (int x = 0; x < kScreenWidth; ++x) {
        uint8_t idx = 0;
        if (lcdOn && (s.lcdc & kLcdcBgEnable)) {
            const int sx = (s.scx + x) & 0xFF;
            const int sy = (s.scy + ly) & 0xFF;
            const uint16_t mapBase =
                (s.lcdc & kLcdcBgMapHi) ? 0x1C00 : 0x1800;
            idx = tileIndexAt(s, tileDataOffset(s.lcdc,
                    s.vram[mapBase + (sy / 8) * 32 + (sx / 8)]),
                    sx & 7, sy & 7);
        }
        const uint8_t shade =
            static_cast<uint8_t>((s.bgp >> (idx * 2)) & 3);
        rgba[x][0] = rgba[x][1] = rgba[x][2] = kGray[shade];
        rgba[x][3] = 255;
    }

    // Window pass: overwrite x >= WX-7 with unscrolled map content.
    if (!windowActive(s, ly)) return;
    const int startX = static_cast<int>(s.wx) - 7;
    if (startX < 0 || startX >= kScreenWidth) return;  // hidden (WX<7)
    const uint16_t mapBase = winMapBase(s.lcdc);
    const int contentRow = ((windowLine / 8) & 31) * 32;
    for (int x = startX; x < kScreenWidth; ++x) {
        const int wx = x - startX;
        const uint8_t indexByte =
            s.vram[mapBase + contentRow + (wx / 8)];
        const uint8_t idx = tileIndexAt(s,
            tileDataOffset(s.lcdc, indexByte), wx & 7, windowLine & 7);
        const uint8_t shade =
            static_cast<uint8_t>((s.bgp >> (idx * 2)) & 3);
        rgba[x][0] = rgba[x][1] = rgba[x][2] = kGray[shade];
        rgba[x][3] = 255;
    }
//@LABS-STUB
    // TODO(5): render BG first (as exercise 04), then overwrite the
    // window region x >= WX-7 using `windowLine` as content row.
    (void)s;
    (void)ly;
    (void)windowLine;
    (void)rgba;
//@LABS-END
}

// Full frame. Track the internal window line: start at 0 and increment
// AFTER each scanline where the window drew. LCD off => white frame.
void renderFrame(const PpuState& s, Frame frame) {
//@LABS-BEGIN 6
//@LABS-SOLUTION
    int windowLine = 0;
    for (int ly = 0; ly < kScreenHeight; ++ly) {
        renderScanline(s, ly, windowLine, frame[ly]);
        if (windowActive(s, ly)) ++windowLine;
    }
//@LABS-STUB
    // TODO(6): loop lines, maintain the window line counter, call
    // renderScanline.
    (void)s;
    (void)frame;
//@LABS-END
}

// Load a snapshot image (8198 bytes; layout in exercise 04's ppu.hpp).
inline bool loadState(const std::string& path, PpuState& out) {
//@LABS-BEGIN 7
//@LABS-SOLUTION
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    uint8_t buf[0x2006];
    const size_t got = std::fread(buf, 1, sizeof(buf), f);
    std::fclose(f);
    if (got != sizeof(buf)) return false;
    for (size_t i = 0; i < 0x2000; ++i) out.vram[i] = buf[i];
    out.lcdc = buf[0x2000];
    out.bgp = buf[0x2001];
    out.scy = buf[0x2002];
    out.scx = buf[0x2003];
    out.wy = buf[0x2004];
    out.wx = buf[0x2005];
    return true;
//@LABS-STUB
    // TODO(7): exact-size read into VRAM + six registers.
    (void)path;
    (void)out;
    return false;
//@LABS-END
}

}  // namespace gbwin
