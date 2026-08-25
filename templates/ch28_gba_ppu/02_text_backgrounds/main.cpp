#define LABSTEST_MAIN
#include <vector>
#include "labstest.hpp"
#include "text_bg.hpp"

using namespace gba;

namespace {

// Build a tiny 1x1-tile world: tile 1 = checker of colors 1/2 (bank 3),
void setup_map(PpuMemory& m) {
    u32 tiles = 0;      // char base block 0
    u32 map = 0x800;    // screen base block 1

    // Tile 1 lives at byte offset 32: rows of nibbles 1,1,1,1 / 2,2,2,2.
    for (int row = 0; row < 8; ++row) {
        int lo = row < 4 ? 1 : 2;
        int hi = row < 4 ? 1 : 2;
        u8 v = u8(lo | (hi << 4));
        m.vram[tiles + 32 + row * 4 + 0] = v;
        m.vram[tiles + 32 + row * 4 + 1] = v;
        m.vram[tiles + 32 + row * 4 + 2] = v;
        m.vram[tiles + 32 + row * 4 + 3] = v;
    }
    for (int ty = 0; ty < 32; ++ty)
        for (int tx = 0; tx < 32; ++tx) {
            bool hole = (tx == 5 && ty == 5);
            u16 entry = u16(hole ? 0 : 1);
            m.wr16(PpuMemory::kVramBase + map + u32(ty) * 64 + u32(tx) * 2,
                   entry);
        }
}

TextBgConfig default_cfg() {
    TextBgConfig c;
    c.priority = 0;
    c.char_base = 0;
    c.screen_base = 0x800;
    c.map_w_tiles = 32;
    c.map_h_tiles = 32;
    return c;
}

}  // namespace

TEST(bgcnt, decode_fields_and_sizes) {
    // prio 2, char base 2, mosaic, 8bpp, screen base 5, size 512x256.
    u16 cnt = u16(2 | (2 << 2) | (1 << 6) | (1 << 7) | (5 << 8) |
                  (1 << 14));
    TextBgConfig c = decode_text_bg_config(cnt);
    EXPECT_EQ(c.priority, 2);
    EXPECT_EQ(c.char_base, 0x8000);
    EXPECT_TRUE(c.mosaic);
    EXPECT_TRUE(c.bpp8);
    EXPECT_EQ(c.screen_base, 0x2800);
    EXPECT_TRUE(c.map_w_tiles == 64 && c.map_h_tiles == 32);

    TextBgConfig tall = decode_text_bg_config(u16(2 << 14));
    EXPECT_TRUE(tall.map_w_tiles == 32 && tall.map_h_tiles == 64);
    EXPECT_EQ(get_text_bg_config(PpuMemory{}, 0).priority, 0);
}

TEST(screen_entry, decode) {
    ScreenEntry e = decode_screen_entry(u16(7 | (1 << 10) | (1 << 11) |
                                             (9 << 12)));
    EXPECT_EQ(e.tile(), 7);
    EXPECT_TRUE(e.hflip());
    EXPECT_TRUE(e.vflip());
    EXPECT_EQ(e.bank(), 9);
}

TEST(tile4bpp, nibble_rows_and_transparency) {
    PpuMemory m;
    // Tile 0 bytes: first row low/high nibbles 1 and 2, second row all zero.
    m.vram[0] = 0x21;
    m.vram[1] = 0x21;
    m.vram[2] = 0x21;
    m.vram[3] = 0x21;
    EXPECT_EQ(tile_pixel(m, 0, 0, 0, false, 3), 3 * 16 + 1);  // low nibble
    EXPECT_EQ(tile_pixel(m, 0, 1, 0, false, 3), 3 * 16 + 2);  // high nibble
    EXPECT_EQ(tile_pixel(m, 0, 0, 1, false, 3), -1);          // transparent
    EXPECT_EQ(tile_pixel(m, 0, 8, 0, false, 3), -1);          // outside tile
}

TEST(text_bg, pixel_lookup_with_scroll_wrap) {
    PpuMemory m;
    setup_map(m);
    TextBgConfig cfg = default_cfg();

    // No scroll: pixel (40,40) hits tile (5,5) -> the transparent hole.
    EXPECT_EQ(text_bg_pixel(m, cfg, 40, 40), -1);
    // Pixel (48,48) -> tile (6,6), row 0 color 1 in bank 0.
    EXPECT_EQ(text_bg_pixel(m, cfg, 48, 48), 1);

    // Scroll by 512+8: masked to 8 -> same column shifted one tile right.
    m.wr16(PpuMemory::kIoBase + 0x10, 520);  // HOFS wraps to 8
    EXPECT_EQ(text_bg_pixel(m, cfg, 32, 40), -1);  // now over the hole
}

TEST(text_bg, flips_change_local_pixel) {
    PpuMemory m;
    setup_map(m);
    TextBgConfig cfg = default_cfg();
    // Overwrite the entry covering screen (48,48) with an h+v flipped tile 1.
    m.wr16(PpuMemory::kVramBase + 0x800 + 6 * 64 + 6 * 2,
           u16(1 | (1 << 10) | (1 << 11)));
    // Unflipped row 0 gives color 1; flipped samples row 7 -> color 2.
    EXPECT_EQ(text_bg_pixel(m, cfg, 48, 48), 2);
}

TEST(compose, priority_and_tie_break) {
    PpuMemory m;
    setup_map(m);

    TextBgConfig front = default_cfg();
    TextBgConfig back = default_cfg();
    back.priority = 1;

    std::vector<TextBgConfig> bgs = {front, back};
    int line[kScreenW];
    compose_text_scanline(m, bgs, 48, line);
    // Front BG shows color 1 at x=48 (tile 6).
    EXPECT_EQ(line[48], 1);
    // At y=48 both layers show plain tile content here -> front wins.
    EXPECT_EQ(line[40], 1);

    // Give only the BACK layer content visible where the front is
    // transparent: shift back's scroll so its hole moves elsewhere.
    // (compose reads IO scroll shared by both; instead drop front content by
    // making the front config point at an empty map.)
    front.screen_base = 0x1000;  // untouched map = all zeros = transparent
    compose_text_scanline(m, bgs, 48, line);
    EXPECT_EQ(line[48], 1);  // back layer now visible through the hole
    // Equal priorities keep the earlier BG: make back equal-priority but
    // empty and front non-empty again.
    back.priority = 0;
    front.screen_base = 0x800;
    back.screen_base = 0x1000;
    compose_text_scanline(m, bgs, 48, line);
    EXPECT_EQ(line[48], 1);  // still front's tile
}
