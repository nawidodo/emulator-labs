// ppu.hpp — complete scanline renderer over a PPU state file v2 snapshot.
//
// Combines chapter 14's background/window passes with chapter 15's sprite
// compositing. Per visible line:
//   1. BG fetch with SCX/SCY wrap (chapter 14 rules),
//   2. window overwrite for x >= WX-7 using its INTERNAL content-line
//      counter (only advances on lines where the window actually drew),
//   3. sprite pass: first <=10 covering OAM entries, per-column winner,
//      transparency at index 0, 0x80 priority vs BG color index.
//
// The `windowOn` mask in renderFrame is a course extension (runner flag
// --window-off-lines A:B): it models the window enable bit being toggled
// mid-frame. Lines where it is false do not draw AND do not advance the
// window's internal counter — the correct hardware skip behavior.
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

namespace gbppu2 {

constexpr int kScreenWidth = 160;
constexpr int kScreenHeight = 144;
constexpr int kOamEntries = 40;
constexpr int kMaxSpritesPerLine = 10;

constexpr uint8_t kLcdcLcdOn = 0x80;
constexpr uint8_t kLcdcWinMapHi = 0x40;
constexpr uint8_t kLcdcWinEnable = 0x20;
constexpr uint8_t kLcdcTileUnsigned = 0x10;
constexpr uint8_t kLcdcBgMapHi = 0x08;
constexpr uint8_t kLcdcSpriteSize = 0x04;
constexpr uint8_t kLcdcSpritesEnable = 0x02;
constexpr uint8_t kLcdcBgEnable = 0x01;

constexpr uint8_t kFlagBgPriority = 0x80;
constexpr uint8_t kFlagYFlip = 0x40;
constexpr uint8_t kFlagXFlip = 0x20;
constexpr uint8_t kFlagOBP1 = 0x10;

// Snapshot image "PPU state file v2" (little-endian), see sprites.hpp in
// exercise 01 for the full layout table.
constexpr unsigned kSnapshotSize = 0x20AA;

struct PpuState {
    uint8_t vram[0x2000];
    uint8_t oam[0xA0];
    uint8_t lcdc, stat, bgp, obp0, obp1, scy, scx, wy, wx, lyc;
};

using Frame = uint8_t[kScreenHeight][kScreenWidth][4];

constexpr uint8_t kGray[4] = {255, 192, 96, 0};

struct Sprite {
    uint8_t y, x, tile, flags;
};

// --- timing model (course simplification, mirrors exercise 02) ----------
constexpr int kDotsPerLine = 456;
constexpr int kVisibleLines = 144;
constexpr int kFrameDots = 70224;
constexpr int kMode2End = 80;
constexpr int kMode3End = 252;

inline int modeAt(int line, int dot) {
    if (line >= kVisibleLines) return 1;
    if (dot < kMode2End) return 2;
    if (dot < kMode3End) return 3;
    return 0;
}

// Deterministic mode-transition trace over one frame; format identical to
// exercise 02's buildModeTrace ("ly=<n> dot=<n> mode=<m>\n" per change).
inline std::string buildModeTrace() {
    std::string out;
    int prevLy = -1;
    int prevMode = -1;
    for (int d = 0; d < kFrameDots; ++d) {
        const int ly = d / kDotsPerLine;
        const int dot = d % kDotsPerLine;
        const int m = modeAt(ly, dot);
        if (ly != prevLy || m != prevMode) {
            out += "ly=" + std::to_string(ly) +
                   " dot=" + std::to_string(dot) +
                   " mode=" + std::to_string(m) + "\n";
            prevLy = ly;
            prevMode = m;
        }
    }
    return out;
}

// --- helpers -------------------------------------------------------------

// Screen x where the window starts, or -1 when hidden entirely
// (WX < 7 quirk; WX >= 167 treated as off-screen in this model).
inline int windowStartX(const PpuState& s) {
    const int sx = static_cast<int>(s.wx) - 7;
    return (sx >= 0 && sx < kScreenWidth) ? sx : -1;
}

// Window draws on this line iff LCD on, LCDC bit5 set, the per-line master
// enable allows it, LY >= WY and the start x is on-screen.
inline bool windowActive(const PpuState& s, int ly, bool lineEnabled) {
    return windowStartX(s) >= 0 && (s.lcdc & kLcdcLcdOn) != 0 &&
           (s.lcdc & kLcdcWinEnable) != 0 && lineEnabled && ly >= s.wy;
}

// VRAM offset of a tile: unsigned $8000 / signed $8800 addressing.
inline uint16_t tileDataOffset(uint8_t lcdc, uint8_t index) {
    if (lcdc & kLcdcTileUnsigned)
        return static_cast<uint16_t>(index * 16);
    return static_cast<uint16_t>(
        0x1000 + static_cast<int>(static_cast<int8_t>(index)) * 16);
}

// Two-plane pixel decode at tile offset (bit 7 is pixel x=0).
inline uint8_t tilePixel(const PpuState& s, uint16_t tileOff, int px,
                         int py) {
    const uint8_t lo = s.vram[tileOff + 2 * py];
    const uint8_t hi = s.vram[tileOff + 2 * py + 1];
    return static_cast<uint8_t>(((lo >> (7 - px)) & 1) |
                                (((hi >> (7 - px)) & 1) << 1));
}

// Load a snapshot image; returns false if unreadable or wrong size.
inline bool loadState(const std::string& path, PpuState& out) {
//@LABS-BEGIN 1
//@LABS-SOLUTION
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    uint8_t buf[kSnapshotSize];
    const size_t got = std::fread(buf, 1, sizeof(buf), f);
    std::fclose(f);
    if (got != kSnapshotSize) return false;
    for (size_t i = 0; i < 0x2000; ++i) out.vram[i] = buf[i];
    for (size_t i = 0; i < 0xA0; ++i) out.oam[i] = buf[0x2000 + i];
    out.lcdc = buf[0x20A0];
    out.stat = buf[0x20A1];
    out.bgp = buf[0x20A2];
    out.obp0 = buf[0x20A3];
    out.obp1 = buf[0x20A4];
    out.scy = buf[0x20A5];
    out.scx = buf[0x20A6];
    out.wy = buf[0x20A7];
    out.wx = buf[0x20A8];
    out.lyc = buf[0x20A9];
    return true;
//@LABS-STUB
    // TODO(1): open the file binary, require exactly kSnapshotSize bytes,
    // then copy VRAM, OAM and the ten registers into `out`.
    (void)path;
    (void)out;
    return false;
//@LABS-END
}

// Pass 1+2: BG color indices and BGP-shaded pixels for one scanline, then
// the window overwrite for x >= WX-7 (unscrolled map, content row =
// `windowLine`). LCD off or BG disabled => indices 0 / shade 0.
void renderBgWindowScanline(const PpuState& s, int ly, bool winActive,
                            int windowLine, uint8_t bgIndex[kScreenWidth],
                            uint8_t bgShade[kScreenWidth]) {
//@LABS-BEGIN 2
//@LABS-SOLUTION
    const bool lcdOn = (s.lcdc & kLcdcLcdOn) != 0;
    const bool bgOn = lcdOn && (s.lcdc & kLcdcBgEnable) != 0;
    const uint16_t bgMap =
        (s.lcdc & kLcdcBgMapHi) ? 0x1C00 : 0x1800;

    for (int x = 0; x < kScreenWidth; ++x) {
        uint8_t idx = 0;
        if (bgOn) {
            const int sx = (s.scx + x) & 0xFF;
            const int sy = (s.scy + ly) & 0xFF;
            const uint8_t entry = s.vram[bgMap + (sy / 8) * 32 + (sx / 8)];
            idx = tilePixel(s, tileDataOffset(s.lcdc, entry), sx & 7, sy & 7);
        }
        bgIndex[x] = idx;
        bgShade[x] = static_cast<uint8_t>((s.bgp >> (idx * 2)) & 3);
    }

    if (!winActive) return;
    const int startX = windowStartX(s);
    const uint16_t winMap =
        (s.lcdc & kLcdcWinMapHi) ? 0x1C00 : 0x1800;
    const int row = ((windowLine / 8) & 31) * 32;
    for (int x = startX; x < kScreenWidth; ++x) {
        const int wxLocal = x - startX;
        const uint8_t entry = s.vram[winMap + row + (wxLocal / 8)];
        const uint8_t idx = tilePixel(s, tileDataOffset(s.lcdc, entry),
                                      wxLocal & 7, windowLine & 7);
        bgIndex[x] = idx;
        bgShade[x] = static_cast<uint8_t>((s.bgp >> (idx * 2)) & 3);
    }
//@LABS-STUB
    // TODO(2): fetch the scrolled BG per pixel (chapter 14 rules), then
    // overwrite x >= WX-7 from the unscrolled window map using
    // `windowLine` as the content row. Write both the color INDEX and the
    // BGP SHADE arrays.
    (void)s;
    (void)ly;
    (void)winActive;
    (void)windowLine;
    (void)bgIndex;
    (void)bgShade;
//@LABS-END
}

// Pass 3: sprite compositing onto one scanline. Same rules as exercise 01
// but operating on precomputed BG/window indices and shades.
void compositeScanline(const PpuState& s, int ly,
                       const uint8_t bgIndex[kScreenWidth],
                       const uint8_t bgShade[kScreenWidth],
                       uint8_t rgba[kScreenWidth][4]) {
//@LABS-BEGIN 3
//@LABS-SOLUTION
    const bool lcdOn = (s.lcdc & kLcdcLcdOn) != 0;
    const bool sprOn = lcdOn && (s.lcdc & kLcdcSpritesEnable) != 0;
    const int height = (s.lcdc & kLcdcSpriteSize) ? 16 : 8;

    for (int x = 0; x < kScreenWidth; ++x) {
        rgba[x][0] = rgba[x][1] = rgba[x][2] = kGray[bgShade[x]];
        rgba[x][3] = 255;
    }
    if (!sprOn) return;

    // First <=10 covering entries in OAM order (x==0 skipped pre-limit).
    Sprite list[kMaxSpritesPerLine];
    int n = 0;
    for (int i = 0; i < kOamEntries && n < kMaxSpritesPerLine; ++i) {
        const Sprite sp{s.oam[4 * i], s.oam[4 * i + 1], s.oam[4 * i + 2],
                        s.oam[4 * i + 3]};
        if (sp.x == 0) continue;
        const int top = static_cast<int>(sp.y) - 16;
        if (ly < top || ly >= top + height) continue;
        list[n++] = sp;
    }

    // Winner per column: smaller x wins, tie keeps lower OAM index.
    Sprite winners[kScreenWidth];
    bool have[kScreenWidth] = {};
    for (int i = 0; i < n; ++i) {
        const Sprite sp = list[i];
        const int left = static_cast<int>(sp.x) - 8;
        const int x0 = left < 0 ? 0 : left;
        const int x1 =
            left + height < kScreenWidth ? left + height : kScreenWidth;
        for (int x = x0; x < x1; ++x)
            if (!have[x] || sp.x < winners[x].x) {
                winners[x] = sp;
                have[x] = true;
            }
    }

    for (int x = 0; x < kScreenWidth; ++x) {
        if (!have[x]) continue;
        const Sprite sp = winners[x];
        int row = ly - (static_cast<int>(sp.y) - 16);
        if (sp.flags & kFlagYFlip) row = height - 1 - row;
        int col = x - (static_cast<int>(sp.x) - 8);
        if (sp.flags & kFlagXFlip) col = 7 - col;
        uint8_t tile = sp.tile;
        if (height == 16)
            tile = static_cast<uint8_t>((tile & 0xFE) | (row >= 8 ? 1 : 0));
        const uint8_t idx =
            tilePixel(s, static_cast<uint16_t>(tile * 16 + 2 * (row & 7)),
                      col, 0);
        const bool bgWins = (sp.flags & kFlagBgPriority) && bgIndex[x] != 0;
        if (idx == 0 || bgWins) continue;
        const uint8_t pal = (sp.flags & kFlagOBP1) ? s.obp1 : s.obp0;
        const uint8_t shade = static_cast<uint8_t>((pal >> (idx * 2)) & 3);
        rgba[x][0] = rgba[x][1] = rgba[x][2] = kGray[shade];
    }
//@LABS-STUB
    // TODO(3): start from the shaded background, then overlay sprites with
    // the exercise-01 rules: <=10 candidates in OAM order, per-column
    // winner, flips, 8x16 pairing, priority vs bgIndex, OBP0/OBP1.
    (void)s;
    (void)ly;
    (void)bgIndex;
    (void)bgShade;
    for (int x = 0; x < kScreenWidth; ++x) {
        rgba[x][0] = rgba[x][1] = rgba[x][2] = 0;
        rgba[x][3] = 255;
    }
//@LABS-END
}

// Full frame: maintain the window's internal content-line counter — it
// advances only on lines where the window DREW, so a mid-frame disable
// (windowOn mask) skips content rows instead of restarting at WY.
// LCD off => white frame (shade 0 everywhere, sprites included).
void renderFrame(const PpuState& s, Frame frame,
                 const bool windowOn[kScreenHeight]) {
//@LABS-BEGIN 4
//@LABS-SOLUTION
    int windowLine = 0;
    for (int ly = 0; ly < kScreenHeight; ++ly) {
        uint8_t bgIndex[kScreenWidth];
        uint8_t bgShade[kScreenWidth];
        const bool wa = windowActive(s, ly, windowOn[ly]);
        renderBgWindowScanline(s, ly, wa, wa ? windowLine : -1, bgIndex,
                               bgShade);
        compositeScanline(s, ly, bgIndex, bgShade, frame[ly]);
        if (wa) ++windowLine;
    }
//@LABS-STUB
    // TODO(4): loop the 144 visible lines, track the window's internal
    // content-line counter (advance ONLY on lines where the window drew),
    // run renderBgWindowScanline then compositeScanline.
    (void)s;
    (void)frame;
    (void)windowOn;
//@LABS-END
}

}  // namespace gbppu2
