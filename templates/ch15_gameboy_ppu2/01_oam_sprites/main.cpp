// Tests for exercise 01: OAM scanning and sprite scanline compositing.
#define LABSTEST_MAIN
#include <cstring>

#include "labstest.hpp"
#include "sprites.hpp"

namespace {

using gbspr::PpuState;
using gbspr::Sprite;

// A fresh state: LCD on, $8000 tiles, BG + sprites enabled, 8x8 sprites,
// identity BGP, OBP0 = identity (index i -> shade i), OBP1 = inverted.
PpuState baseState() {
    PpuState s{};
    s.lcdc = gbspr::kLcdcLcdOn | gbspr::kLcdcTileUnsigned |
             gbspr::kLcdcSpritesEnable | gbspr::kLcdcBgEnable;
    s.stat = 0x00;
    s.bgp = 0xE4;  // identity: index -> same shade
    s.obp0 = 0xE4;
    s.obp1 = 0x1B;  // index 1 -> shade 2, index 2 -> shade 1, 3 -> 0
    return s;
}

// Solid tile: every pixel the given color index, written at VRAM offset
// tile*16.
void putSolidTile(PpuState& s, uint8_t tile, uint8_t idx) {
    const uint8_t lo = idx & 1 ? 0xFF : 0x00;
    const uint8_t hi = idx & 2 ? 0xFF : 0x00;
    for (int r = 0; r < 8; ++r) {
        s.vram[tile * 16 + 2 * r] = lo;
        s.vram[tile * 16 + 2 * r + 1] = hi;
    }
}

// Asymmetric 8x16 sprite pair: top tile has a single dark pixel in its
// top-left corner, bottom tile in its bottom-right corner.
void putTallTilePair(PpuState& s, uint8_t firstTile) {
    const uint8_t top = firstTile & 0xFE;
    const uint8_t bot = firstTile | 1;
    std::memset(&s.vram[top * 16], 0, 16);
    std::memset(&s.vram[bot * 16], 0, 16);
    // Top half: plane0 row0 = 0x80 -> pixel (0,0) index 1.
    s.vram[top * 16 + 0] = 0x80;
    // Bottom half: plane0 row7 = 0x01 -> pixel (7,7) index 1.
    s.vram[bot * 16 + 14] = 0x01;
}

void setOam(PpuState& s, int entry, uint8_t y, uint8_t x, uint8_t tile,
            uint8_t flags) {
    s.oam[4 * entry + 0] = y;
    s.oam[4 * entry + 1] = x;
    s.oam[4 * entry + 2] = tile;
    s.oam[4 * entry + 3] = flags;
}

uint8_t shadeAt(const uint8_t rgba[160][4], int x) {
    // Invert the ramp {255,192,96,0} back to a shade number.
    for (int sh = 0; sh < 4; ++sh)
        if (rgba[x][0] == gbspr::kGray[sh]) return static_cast<uint8_t>(sh);
    return 99;
}

}  // namespace

TEST(oam, height_follows_lcdc_bit2) {
    EXPECT_EQ(gbspr::spriteHeight(0x00), 8);
    EXPECT_EQ(gbspr::spriteHeight(0x83), 8);
    EXPECT_EQ(gbspr::spriteHeight(0x84), 16);
    EXPECT_EQ(gbspr::spriteHeight(0x87), 16);
}

TEST(oam, collect_keeps_covering_entries_in_order) {
    PpuState s = baseState();
    putSolidTile(s, 2, 3);
    // Entry 0 does NOT cover line 60 (y=30 -> lines 14..21); entries
    // 1..3 do (y=70/71/69 cover lines 54..61, 55..62 and 53..60).
    setOam(s, 0, 30, 20, 2, 0);
    setOam(s, 1, 70, 40, 2, 0);
    setOam(s, 2, 71, 80, 2, 0);
    setOam(s, 3, 69, 120, 2, 0);
    Sprite out[10] = {};
    const int n =
        gbspr::collectSpritesForLine(s.oam, 60, s.lcdc, out);
    EXPECT_EQ(n, 3);
    EXPECT_EQ(out[0].x, 40);
    EXPECT_EQ(out[1].x, 80);
    EXPECT_EQ(out[2].x, 120);
}

TEST(oam, collect_stops_at_ten_in_oam_order) {
    PpuState s = baseState();
    for (int i = 0; i < 12; ++i) setOam(s, i, 70, 16 + 8 * i, 2, 0);
    Sprite out[10] = {};
    const int n =
        gbspr::collectSpritesForLine(s.oam, 60, s.lcdc, out);
    EXPECT_EQ(n, 10);
    // First ten IN OAM ORDER — not the ten with the best x.
    EXPECT_EQ(out[0].x, 16);
    EXPECT_EQ(out[9].x, 88);
}

TEST(oam, collect_skips_x0_before_limit) {
    PpuState s = baseState();
    setOam(s, 0, 70, 0, 2, 0);     // x==0: hidden, must NOT consume a slot
    for (int i = 1; i <= 10; ++i)  // these ten fill the limit exactly
        setOam(s, i, 70, 16 + 8 * i, 2, 0);
    Sprite out[10] = {};
    const int n =
        gbspr::collectSpritesForLine(s.oam, 60, s.lcdc, out);
    EXPECT_EQ(n, 10);
    EXPECT_EQ(out[0].x, 24);  // entry 1 is the first collected
}

TEST(oam, collect_respects_height_16_range) {
    PpuState s = baseState();
    s.lcdc |= gbspr::kLcdcSpriteSize;  // 8x16
    setOam(s, 0, 40, 50, 0x40, 0);     // lines 24..39
    Sprite out[10] = {};
    EXPECT_EQ(gbspr::collectSpritesForLine(s.oam, 23, s.lcdc, out), 0);
    EXPECT_EQ(gbspr::collectSpritesForLine(s.oam, 24, s.lcdc, out), 1);
    EXPECT_EQ(gbspr::collectSpritesForLine(s.oam, 39, s.lcdc, out), 1);
    EXPECT_EQ(gbspr::collectSpritesForLine(s.oam, 40, s.lcdc, out), 0);
}

TEST(compose, background_shows_where_no_sprite) {
    PpuState s = baseState();
    putSolidTile(s, 2, 3);
    setOam(s, 0, 70, 40, 2, 0);  // columns 32..39 only
    uint8_t bg[160];
    uint8_t rgba[160][4];
    for (int x = 0; x < 160; ++x) bg[x] = 1;  // BG index 1 everywhere
    gbspr::renderSpritesScanline(s, 60, bg, rgba);
    EXPECT_EQ(shadeAt(rgba, 0), 1);   // BGP field of index 1
    EXPECT_EQ(shadeAt(rgba, 31), 1);
    EXPECT_EQ(shadeAt(rgba, 40), 1);  // past right edge
}

TEST(compose, opaque_sprite_pixel_replaces_background) {
    PpuState s = baseState();
    putSolidTile(s, 2, 3);
    setOam(s, 0, 70, 40, 2, 0);
    uint8_t bg[160];
    uint8_t rgba[160][4];
    for (int x = 0; x < 160; ++x) bg[x] = 1;
    gbspr::renderSpritesScanline(s, 60, bg, rgba);
    for (int x = 32; x < 40; ++x) EXPECT_EQ(shadeAt(rgba, x), 3);
}

TEST(compose, color_index_zero_is_transparent) {
    PpuState s = baseState();
    putSolidTile(s, 2, 0);  // whole tile is index 0
    setOam(s, 0, 70, 40, 2, 0);
    uint8_t bg[160];
    uint8_t rgba[160][4];
    for (int x = 0; x < 160; ++x) bg[x] = 2;
    gbspr::renderSpritesScanline(s, 60, bg, rgba);
    for (int x = 32; x < 40; ++x) EXPECT_EQ(shadeAt(rgba, x), 2);
}

TEST(compose, bg_priority_hides_only_on_nonzero_index) {
    PpuState s = baseState();
    putSolidTile(s, 2, 3);
    setOam(s, 0, 70, 40, 2, gbspr::kFlagBgPriority);
    uint8_t bg[160];
    uint8_t rgba[160][4];

    // BG index 1 under the sprite => flag hides it.
    for (int x = 0; x < 160; ++x) bg[x] = 1;
    gbspr::renderSpritesScanline(s, 60, bg, rgba);
    for (int x = 32; x < 40; ++x) EXPECT_EQ(shadeAt(rgba, x), 1);

    // BG index 0 under the sprite => sprite still shows.
    for (int x = 0; x < 160; ++x) bg[x] = 0;
    gbspr::renderSpritesScanline(s, 60, bg, rgba);
    for (int x = 32; x < 40; ++x) EXPECT_EQ(shadeAt(rgba, x), 3);
}

TEST(compose, x_flip_reverses_row_bits) {
    PpuState s = baseState();
    // Tile with a single index-1 pixel at x=0 of every row.
    std::memset(&s.vram[2 * 16], 0, 16);
    for (int r = 0; r < 8; ++r) s.vram[2 * 16 + 2 * r] = 0x80;
    setOam(s, 0, 70, 40, 2, gbspr::kFlagXFlip);
    uint8_t bg[160];
    uint8_t rgba[160][4];
    for (int x = 0; x < 160; ++x) bg[x] = 0;
    gbspr::renderSpritesScanline(s, 60, bg, rgba);
    EXPECT_EQ(shadeAt(rgba, 32), 0);  // unflipped x=0 column now blank
    EXPECT_EQ(shadeAt(rgba, 39), 1);  // flipped pixel lands at x=7
}

TEST(compose, y_flip_picks_opposite_row_and_8x16_pairing) {
    PpuState s = baseState();
    s.lcdc |= gbspr::kLcdcSpriteSize;
    putTallTilePair(s, 0x40);
    setOam(s, 0, 40, 40, 0x40, 0);  // screen lines 24..39, columns 32..39

    uint8_t bg[160];
    uint8_t rgba[160][4];
    for (int x = 0; x < 160; ++x) bg[x] = 0;

    // No flip: dark pixel at line 24 (top of top half).
    gbspr::renderSpritesScanline(s, 24, bg, rgba);
    EXPECT_EQ(shadeAt(rgba, 32), 1);
    // Bottom half uses tile|1: dark pixel at line 39.
    gbspr::renderSpritesScanline(s, 39, bg, rgba);
    EXPECT_EQ(shadeAt(rgba, 39), 1);

    // Y flip: line 24 now samples bottom half's LAST row (dark pixel at
    // x=7), line 39 samples top half's first row.
    setOam(s, 0, 40, 40, 0x40, gbspr::kFlagYFlip);
    gbspr::renderSpritesScanline(s, 24, bg, rgba);
    EXPECT_EQ(shadeAt(rgba, 32), 0);
    EXPECT_EQ(shadeAt(rgba, 39), 1);
    gbspr::renderSpritesScanline(s, 39, bg, rgba);
    EXPECT_EQ(shadeAt(rgba, 32), 1);
    EXPECT_EQ(shadeAt(rgba, 39), 0);
}

TEST(compose, palette_flag_selects_obp1) {
    PpuState s = baseState();
    putSolidTile(s, 2, 1);  // index 1: shade 1 via OBP0, shade 2 via OBP1
    setOam(s, 0, 70, 40, 2, 0);
    uint8_t bg[160];
    uint8_t rgba[160][4];
    for (int x = 0; x < 160; ++x) bg[x] = 0;
    gbspr::renderSpritesScanline(s, 60, bg, rgba);
    EXPECT_EQ(shadeAt(rgba, 35), 1);

    setOam(s, 0, 70, 40, 2, gbspr::kFlagOBP1);
    gbspr::renderSpritesScanline(s, 60, bg, rgba);
    EXPECT_EQ(shadeAt(rgba, 35), 2);  // OBP1 = 0x1B maps 1 -> 2
}

TEST(compose, smaller_x_wins_then_lower_oam_index) {
    PpuState s = baseState();
    putSolidTile(s, 2, 1);
    putSolidTile(s, 3, 3);
    // Overlapping columns 36..39: entry 1 has smaller x and must win there.
    setOam(s, 0, 70, 44, 2, 0);  // columns 36..43
    setOam(s, 1, 70, 40, 3, 0);  // columns 32..39
    uint8_t bg[160];
    uint8_t rgba[160][4];
    for (int x = 0; x < 160; ++x) bg[x] = 0;
    gbspr::renderSpritesScanline(s, 60, bg, rgba);
    EXPECT_EQ(shadeAt(rgba, 33), 3);  // only entry 1 covers
    EXPECT_EQ(shadeAt(rgba, 37), 3);  // overlap: smaller x (entry 1) wins
    EXPECT_EQ(shadeAt(rgba, 41), 1);  // only entry 0 covers

    // Tie on x: lower OAM index wins even if its tile is "worse".
    setOam(s, 0, 70, 40, 3, 0);
    setOam(s, 1, 70, 40, 2, 0);
    gbspr::renderSpritesScanline(s, 60, bg, rgba);
    EXPECT_EQ(shadeAt(rgba, 35), 3);
}

TEST(compose, limit_beats_priority_order) {
    PpuState s = baseState();
    putSolidTile(s, 2, 1);
    putSolidTile(s, 3, 3);
    // Ten entries at x=100 (tile 1), then an 11th at x=20 (tile 3). The
    // 11th is never evaluated: the limit applies to OAM order BEFORE the
    // per-column priority contest.
    for (int i = 0; i < 10; ++i) setOam(s, i, 70, 100, 2, 0);
    setOam(s, 10, 70, 20, 3, 0);
    uint8_t bg[160];
    uint8_t rgba[160][4];
    for (int x = 0; x < 160; ++x) bg[x] = 0;
    gbspr::renderSpritesScanline(s, 60, bg, rgba);
    EXPECT_EQ(shadeAt(rgba, 15), 0);   // 11th sprite (cols 12..19) ignored
    EXPECT_EQ(shadeAt(rgba, 95), 1);   // first ten drawn at cols 92..99
    EXPECT_EQ(shadeAt(rgba, 100), 0);  // past the first-ten sprites
}
