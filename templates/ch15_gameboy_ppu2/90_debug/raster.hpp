// raster.hpp — PPU II raster helpers under repair in the debugging drill.
//
// FIVE defects are seeded below (STUB side). Each produces plausible-
// looking output, so eyeballing is not enough — isolate each one with the
// failing suites and document it in bug-report.md:
//   bug / root cause / first divergence / fix / regression test.
//
// Suites: debug_modes (defect 1), debug_sprites (defects 2+3),
//         debug_lyc (defect 4), debug_window (defect 5).
#pragma once

#include <cstdint>
#include <string>

namespace gbfix {

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

// Course timing model: mode 2 dots 0..79, mode 3 dots 80..251, mode 0
// dots 252..455, mode 1 across vblank.
constexpr int kDotsPerLine = 456;
constexpr int kVisibleLines = 144;
constexpr int kFrameDots = 70224;
constexpr int kMode2End = 80;
constexpr int kMode3End = 252;

struct PpuState {
    uint8_t vram[0x2000];
    uint8_t oam[0xA0];
    uint8_t lcdc, stat, bgp, obp0, obp1, scy, scx, wy, wx, lyc;
};

using Frame = uint8_t[kScreenHeight][kScreenWidth][4];

struct Sprite {
    uint8_t y, x, tile, flags;
};

constexpr uint8_t kGray[4] = {255, 192, 96, 0};

inline uint16_t tileDataOffset(uint8_t lcdc, uint8_t index) {
    if (lcdc & kLcdcTileUnsigned)
        return static_cast<uint16_t>(index * 16);
    return static_cast<uint16_t>(
        0x1000 + static_cast<int>(static_cast<int8_t>(index)) * 16);
}

inline uint8_t tilePixel(const PpuState& s, uint16_t tileOff, int px,
                         int py) {
    const uint8_t lo = s.vram[tileOff + 2 * py];
    const uint8_t hi = s.vram[tileOff + 2 * py + 1];
    return static_cast<uint8_t>(((lo >> (7 - px)) & 1) |
                                (((hi >> (7 - px)) & 1) << 1));
}

inline int windowStartX(const PpuState& s) {
    const int sx = static_cast<int>(s.wx) - 7;
    return (sx >= 0 && sx < kScreenWidth) ? sx : -1;
}

inline bool windowActive(const PpuState& s, int ly, bool lineEnabled) {
    return windowStartX(s) >= 0 && (s.lcdc & kLcdcLcdOn) != 0 &&
           (s.lcdc & kLcdcWinEnable) != 0 && lineEnabled && ly >= s.wy;
}

// Mode lookup for the transition trace. The trace must show mode 3 ending
// at dot 252 of EVERY visible line — that hblank edge is when VRAM/OAM
// unlock for the CPU.
// Symptom of defect 1: no visible line ever reaches mode 0 in the trace —
// the 3->0 threshold is computed against the wrong line base, so the
// VRAM/OAM unlock lands a full line late (past every visible line).
inline int modeAt(int line, int dot) {
//@LABS-BEGIN 1
//@LABS-STUB
    // TODO(1): BUG(1) — the drawing window is measured from the previous
    // base; dot - kDotsPerLine can never reach 252 inside the current
    // line, so mode 3 -> mode 0 fires only after the line wraps — i.e. a
    // full line late.
    if (line >= kVisibleLines) return 1;
    if (dot < kMode2End) return 2;
    if (dot - kDotsPerLine < kMode3End) return 3;
    return 0;
//@LABS-SOLUTION
    if (line >= kVisibleLines) return 1;
    if (dot < kMode2End) return 2;
    if (dot < kMode3End) return 3;
    return 0;
//@LABS-END
}

// Deterministic mode-transition trace ("ly=<n> dot=<n> mode=<m>\n" per
// (ly, mode) change over single-dot steps).
inline std::string buildModeTrace(int totalDots) {
    std::string out;
    int prevLy = -1;
    int prevMode = -1;
    for (int d = 0; d < totalDots; ++d) {
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

// First <=10 covering OAM entries for a scanline, in OAM order.
// Symptom of defect 2: sprites with x == 0 are DRAWN (shifted on-screen)
// while every normal sprite disappears — the hide condition is inverted.
inline int collectSpritesForLine(const PpuState& s, int ly,
                                 Sprite out[kMaxSpritesPerLine]) {
//@LABS-BEGIN 2
//@LABS-STUB
    // TODO(2): BUG(2) — x==0 hide test inverted — skips visible sprites, keeps
    // hidden ones.
    const int height = (s.lcdc & kLcdcSpriteSize) ? 16 : 8;
    int n = 0;
    for (int i = 0; i < kOamEntries; ++i) {
        const Sprite sp{s.oam[4 * i], s.oam[4 * i + 1], s.oam[4 * i + 2],
                        s.oam[4 * i + 3]};
        if (sp.x != 0) continue;
        const int top = static_cast<int>(sp.y) - 16;
        if (ly < top || ly >= top + height) continue;
        out[n++] = sp;
        if (n == kMaxSpritesPerLine) break;
    }
    return n;
//@LABS-SOLUTION
    const int height = (s.lcdc & kLcdcSpriteSize) ? 16 : 8;
    int n = 0;
    for (int i = 0; i < kOamEntries; ++i) {
        const Sprite sp{s.oam[4 * i], s.oam[4 * i + 1], s.oam[4 * i + 2],
                        s.oam[4 * i + 3]};
        if (sp.x == 0) continue;
        const int top = static_cast<int>(sp.y) - 16;
        if (ly < top || ly >= top + height) continue;
        out[n++] = sp;
        if (n == kMaxSpritesPerLine) break;
    }
    return n;
//@LABS-END
}

// Does the winning sprite's pixel show at this column? Transparent at
// tile index 0; OAM flag 0x80 hides it where the BG color INDEX != 0.
// Symptom of defect 3: exactly inverted — priority-flagged sprites punch
// through nonzero background while unflagged sprites vanish behind any
// nonzero BG pixel.
inline bool spritePixelWins(uint8_t flags, uint8_t spriteIdx,
                            uint8_t bgIndex) {
//@LABS-BEGIN 3
//@LABS-STUB
    // TODO(3): BUG(3) — BG-over-sprite priority sense flipped.
    return spriteIdx != 0 && (flags & kFlagBgPriority) && bgIndex != 0;
//@LABS-SOLUTION
    return spriteIdx != 0 &&
           !((flags & kFlagBgPriority) && bgIndex != 0);
//@LABS-END
}

// LY == LYC coincidence flag feeding STAT bit 2.
// Symptom of defect 4: coincidence asserts one line early — the STAT IRQ
// log shows the interrupt firing at LYC-1 instead of LYC.
inline bool coincidenceFlag(int ly, int lyc) {
//@LABS-BEGIN 4
//@LABS-STUB
    // TODO(4): BUG(4) — compares against the previous line.
    return (ly - 1) == lyc;
//@LABS-SOLUTION
    return ly == lyc;
//@LABS-END
}

// Shared correct glue used by renderFrameWindow below.
namespace glue {

inline void renderScanline(const PpuState& s, int ly, bool wa,
                           int contentRow, uint8_t rgba[kScreenWidth][4]) {
    const bool lcdOn = (s.lcdc & kLcdcLcdOn) != 0;
    const bool bgOn = lcdOn && (s.lcdc & kLcdcBgEnable) != 0;
    const uint16_t bgMap = (s.lcdc & kLcdcBgMapHi) ? 0x1C00 : 0x1800;
    uint8_t bgIndex[kScreenWidth];
    uint8_t shade[kScreenWidth];

    for (int x = 0; x < kScreenWidth; ++x) {
        uint8_t idx = 0;
        if (bgOn) {
            const int sx = (s.scx + x) & 0xFF;
            const int sy = (s.scy + ly) & 0xFF;
            const uint8_t entry =
                s.vram[bgMap + (sy / 8) * 32 + (sx / 8)];
            idx = tilePixel(s, tileDataOffset(s.lcdc, entry), sx & 7,
                            sy & 7);
        }
        bgIndex[x] = idx;
        shade[x] = static_cast<uint8_t>((s.bgp >> (idx * 2)) & 3);
    }

    if (wa) {
        const int startX = windowStartX(s);
        const uint16_t winMap =
            (s.lcdc & kLcdcWinMapHi) ? 0x1C00 : 0x1800;
        const int row = ((contentRow / 8) & 31) * 32;
        for (int x = startX; x < kScreenWidth; ++x) {
            const int wl = x - startX;
            const uint8_t entry = s.vram[winMap + row + (wl / 8)];
            const uint8_t idx = tilePixel(
                s, tileDataOffset(s.lcdc, entry), wl & 7, contentRow & 7);
            bgIndex[x] = idx;
            shade[x] = static_cast<uint8_t>((s.bgp >> (idx * 2)) & 3);
        }
    }

    for (int x = 0; x < kScreenWidth; ++x) {
        rgba[x][0] = rgba[x][1] = rgba[x][2] = kGray[shade[x]];
        rgba[x][3] = 255;
    }

    // Sprites (exercise-01 rules; collect/priority via the functions above).
    if (lcdOn && (s.lcdc & kLcdcSpritesEnable)) {
        const int height = (s.lcdc & kLcdcSpriteSize) ? 16 : 8;
        Sprite list[kMaxSpritesPerLine];
        const int n = collectSpritesForLine(s, ly, list);
        Sprite winners[kScreenWidth];
        bool have[kScreenWidth] = {};
        for (int i = 0; i < n; ++i) {
            const Sprite sp = list[i];
            const int left = static_cast<int>(sp.x) - 8;
            const int x0 = left < 0 ? 0 : left;
            const int x1 = left + height < kScreenWidth ? left + height
                                                        : kScreenWidth;
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
                tile =
                    static_cast<uint8_t>((tile & 0xFE) | (row >= 8 ? 1 : 0));
            const uint8_t idx = tilePixel(
                s, static_cast<uint16_t>(tile * 16 + 2 * (row & 7)), col, 0);
            if (!spritePixelWins(sp.flags, idx, bgIndex[x])) continue;
            const uint8_t pal = (sp.flags & kFlagOBP1) ? s.obp1 : s.obp0;
            const uint8_t sh =
                static_cast<uint8_t>((pal >> (idx * 2)) & 3);
            rgba[x][0] = rgba[x][1] = rgba[x][2] = kGray[sh];
        }
    }
}

}  // namespace glue

// Full frame with a per-line window enable mask (the runner's
// --window-off-lines extension). The window's INTERNAL content counter
// advances only on lines where the window drew — disabling it mid-frame
// SKIPS content rows rather than restarting from WY.
// Symptom of defect 5: content row derived as LY-WY per line, so after a
// mid-frame disable the window restarts at WY instead of skipping.
void renderFrameWindow(const PpuState& s, Frame frame,
                       const bool windowOn[kScreenHeight]) {
//@LABS-BEGIN 5
//@LABS-STUB
    // TODO(5): BUG(5) — window content row computed as ly - WY, not the
    // the internal line counter.
    for (int ly = 0; ly < kScreenHeight; ++ly) {
        const bool wa = windowActive(s, ly, windowOn[ly]);
        const int contentRow = wa ? ly - s.wy : -1;
        glue::renderScanline(s, ly, wa, contentRow, frame[ly]);
    }
//@LABS-SOLUTION
    int windowLine = 0;
    for (int ly = 0; ly < kScreenHeight; ++ly) {
        const bool wa = windowActive(s, ly, windowOn[ly]);
        glue::renderScanline(s, ly, wa, wa ? windowLine : -1, frame[ly]);
        if (wa) ++windowLine;
    }
//@LABS-END
}

}  // namespace gbfix
