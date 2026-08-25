// sprites.hpp — OAM sprite scanning and scanline compositing (PPU II).
//
// OAM holds 40 sprite entries of 4 bytes each: y, x, tile, flags. Sprite
// Y/X are stored with a 16/8-pixel offset: the sprite occupies screen
// lines [y-16, y-16+height) and columns [x-8, x). An entry with x == 0
// sits entirely off-screen and is SKIPPED by the hardware before the
// 10-sprites-per-line limit applies; this model mirrors that (SPEC.md).
//
// OAM flag bits:
//   0x80 BG-over-sprite priority: the sprite pixel is hidden where the
//        final BG/window COLOR INDEX is != 0 (the index, not the shade)
//   0x40 Y flip    0x20 X flip    0x10 palette select (OBP1 when set)
//
// LCDC bits used here:
//   0x80 LCD on   0x04 sprite size 8x16   0x02 sprites enable
#pragma once

#include <cstdint>

namespace gbspr {

constexpr int kScreenWidth = 160;
constexpr int kOamEntries = 40;
constexpr int kMaxSpritesPerLine = 10;

constexpr uint8_t kLcdcLcdOn = 0x80;
constexpr uint8_t kLcdcWinMapHi = 0x40;
constexpr uint8_t kLcdcWinEnable = 0x20;
constexpr uint8_t kLcdcTileUnsigned = 0x10;
constexpr uint8_t kLcdcBgMapHi = 0x08;
constexpr uint8_t kLcdcSpriteSize = 0x04;  // set => 8x16 sprites
constexpr uint8_t kLcdcSpritesEnable = 0x02;
constexpr uint8_t kLcdcBgEnable = 0x01;

constexpr uint8_t kFlagBgPriority = 0x80;
constexpr uint8_t kFlagYFlip = 0x40;
constexpr uint8_t kFlagXFlip = 0x20;
constexpr uint8_t kFlagOBP1 = 0x10;

// Snapshot image "PPU state file v2" (little-endian):
//   0x0000  0x2000  VRAM ($8000-$9FFF)
//   0x2000  0x00A0  OAM ($FE00-$FE9F, 40 entries x 4 bytes)
//   0x20A0  LCDC    0x20A1 STAT   0x20A2 BGP   0x20A3 OBP0  0x20A4 OBP1
//   0x20A5  SCY     0x20A6 SCX    0x20A7 WY    0x20A8 WX    0x20A9 LYC
constexpr unsigned kSnapshotSize = 0x20AA;  // 8192 + 160 + 10

struct PpuState {
    uint8_t vram[0x2000];
    uint8_t oam[0xA0];
    uint8_t lcdc, stat, bgp, obp0, obp1, scy, scx, wy, wx, lyc;
};

struct Sprite {
    uint8_t y, x, tile, flags;
};

// Fixed grayscale ramp (matches chapter 14) so golden hashes are
// platform-independent: shade 0..3 -> RGBA8.
constexpr uint8_t kGray[4] = {255, 192, 96, 0};

// Height in pixels of every sprite: LCDC bit 2 selects 8x16 mode.
inline int spriteHeight(uint8_t lcdc) {
//@LABS-BEGIN 1
//@LABS-SOLUTION
    return (lcdc & kLcdcSpriteSize) ? 16 : 8;
//@LABS-STUB
    // TODO(1): LCDC bit 2 set => 16, else 8.
    (void)lcdc;
    return 8;
//@LABS-END
}

// Scan all 40 OAM entries in OAM order and collect those covering scanline
// `ly`, stopping at 10 — the hardware evaluates only the FIRST ten sprites
// per line, even if a better-positioned sprite comes later in OAM.
// Entries with x == 0 are skipped entirely and do NOT consume a limit slot
// (documented course choice; see SPEC.md).
// Returns how many entries were written to `out`.
inline int collectSpritesForLine(const uint8_t* oam, int ly, uint8_t lcdc,
                                 Sprite out[kMaxSpritesPerLine]) {
//@LABS-BEGIN 2
//@LABS-SOLUTION
    const int height = spriteHeight(lcdc);
    int n = 0;
    for (int i = 0; i < kOamEntries; ++i) {
        const Sprite sp{oam[4 * i + 0], oam[4 * i + 1], oam[4 * i + 2],
                        oam[4 * i + 3]};
        if (sp.x == 0) continue;  // hidden: skipped before the limit
        const int top = static_cast<int>(sp.y) - 16;
        if (ly < top || ly >= top + height) continue;
        out[n++] = sp;
        if (n == kMaxSpritesPerLine) break;
    }
    return n;
//@LABS-STUB
    // TODO(2): walk OAM in order, skip x==0, keep entries whose screen
    // line range [y-16, y-16+height) covers ly, stop at 10.
    (void)oam;
    (void)ly;
    (void)lcdc;
    (void)out;
    return 0;
//@LABS-END
}

// Composite the sprite layer over one scanline. `bgIndices[160]` holds the
// final BG/window COLOR INDEX (0..3) for each column — sprite priority
// compares against the index, not the shaded pixel. Columns without a
// sprite pixel emit the BGP-shaded background.
//
// Rules (hardware model):
//   - a sprite pixel with tile color index 0 is transparent;
//   - smaller X wins a column; ties go to the lower OAM index;
//   - OAM flag 0x80 hides the sprite where the BG index != 0;
//   - X flip reverses the bit order inside the tile row, Y flip picks the
//     row (height-1-in-y);
//   - 8x16 sprites use tile&0xFE for the top half and |1 for the bottom;
//   - flag 0x10 selects OBP1 instead of OBP0 (index 0 is always
//     transparent regardless of palette).
void renderSpritesScanline(const PpuState& s, int ly,
                           const uint8_t bgIndices[kScreenWidth],
                           uint8_t rgba[kScreenWidth][4]) {
//@LABS-BEGIN 3
//@LABS-SOLUTION
    const bool lcdOn = (s.lcdc & kLcdcLcdOn) != 0;
    const bool sprOn = lcdOn && (s.lcdc & kLcdcSpritesEnable) != 0;
    const int height = spriteHeight(s.lcdc);

    // Base layer: BG indices through BGP.
    uint8_t bgShade[kScreenWidth];
    for (int x = 0; x < kScreenWidth; ++x)
        bgShade[x] = static_cast<uint8_t>((s.bgp >> (bgIndices[x] * 2)) & 3);

    // Winner per column: walk the (already limited) OAM-order candidate
    // list; only a STRICTLY smaller x replaces an incumbent, so ties keep
    // the lower OAM index.
    Sprite winners[kScreenWidth];
    bool have[kScreenWidth] = {};
    if (sprOn) {
        Sprite list[kMaxSpritesPerLine];
        const int n = collectSpritesForLine(s.oam, ly, s.lcdc, list);
        for (int i = 0; i < n; ++i) {
            const Sprite sp = list[i];
            const int left = static_cast<int>(sp.x) - 8;
            const int x0 = left < 0 ? 0 : left;
            const int x1 = left + height < kScreenWidth ? left + height
                                                        : kScreenWidth;
            for (int x = x0; x < x1; ++x) {
                if (!have[x] || sp.x < winners[x].x) {
                    winners[x] = sp;
                    have[x] = true;
                }
            }
        }
    }

    for (int x = 0; x < kScreenWidth; ++x) {
        uint8_t shade = bgShade[x];
        if (have[x]) {
            const Sprite sp = winners[x];
            int row = ly - (static_cast<int>(sp.y) - 16);
            if (sp.flags & kFlagYFlip) row = height - 1 - row;
            int col = x - (static_cast<int>(sp.x) - 8);
            if (sp.flags & kFlagXFlip) col = 7 - col;
            uint8_t tile = sp.tile;
            if (height == 16)
                tile = static_cast<uint8_t>((tile & 0xFE) | (row >= 8 ? 1 : 0));
            const int r = row & 7;
            const uint16_t off = static_cast<uint16_t>(tile * 16 + 2 * r);
            const uint8_t idx = static_cast<uint8_t>(
                ((s.vram[off] >> (7 - col)) & 1) |
                (((s.vram[off + 1] >> (7 - col)) & 1) << 1));
            const bool bgWins =
                (sp.flags & kFlagBgPriority) && bgIndices[x] != 0;
            if (idx != 0 && !bgWins) {
                const uint8_t pal =
                    (sp.flags & kFlagOBP1) ? s.obp1 : s.obp0;
                shade = static_cast<uint8_t>((pal >> (idx * 2)) & 3);
            }
        }
        rgba[x][0] = rgba[x][1] = rgba[x][2] = kGray[shade];
        rgba[x][3] = 255;
    }
//@LABS-STUB
    // TODO(3): pick the winning sprite per column (smaller x, then lower
    // OAM index), decode its tile pixel honoring flips/8x16 pairing, apply
    // the BG-priority flag against bgIndices, translate through OBP0/OBP1
    // and emit the grayscale ramp; non-sprite columns keep the BGP-shaded
    // background.
    (void)s;
    (void)ly;
    (void)bgIndices;
    for (int x = 0; x < kScreenWidth; ++x) {
        rgba[x][0] = rgba[x][1] = rgba[x][2] = 0;
        rgba[x][3] = 255;
    }
//@LABS-END
}

}  // namespace gbspr
