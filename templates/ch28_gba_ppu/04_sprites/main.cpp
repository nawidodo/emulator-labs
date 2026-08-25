#define LABSTEST_MAIN
#include <vector>
#include "labstest.hpp"
#include "sprites.hpp"
#include <cstddef>

using namespace gba;

namespace {

ObjAttrs base_sprite() {
    ObjAttrs s;
    s.x = 50;
    s.y = 40;
    s.shape = 0;  // square
    s.size = 1;   // 16x16
    return s;
}


}  // namespace

TEST(objattrs, decode_fields) {
    PpuMemory m;
    u8* o = m.oam;
    // ATTR0: y=100, affine, mode 1, 8bpp, square. ATTR1: x=300, matrix 5,
    // size 2. ATTR2: tile 7, prio 2, bank 3.
    o[0] = 100;
    o[1] = u8(1 | (1 << 2) | (1 << 5));
    o[2] = 300 & 0xFF;
    o[3] = u8(((300 >> 8) & 1) | (5 << 1) | (2 << 6));
    o[4] = 7;
    o[5] = u8((2 << 2) | (3 << 4));
    ObjAttrs s = decode_obj_attrs(m, 0);
    EXPECT_EQ(s.y, 100);
    EXPECT_TRUE(s.affine);
    EXPECT_EQ(s.mode, 1);
    EXPECT_TRUE(s.bpp8);
    EXPECT_EQ(s.x, 300);
    EXPECT_EQ(s.matrix, 5);
    EXPECT_EQ(s.size, 2);
    EXPECT_EQ(s.tile, 7);
    EXPECT_EQ(s.priority, 2);
    EXPECT_EQ(s.bank, 3);
    EXPECT_TRUE(s.width() == 32 && s.height() == 32);  // square size 2
}

TEST(objattrs, shape_size_table) {
    ObjAttrs wide = base_sprite();
    wide.shape = 1;
    wide.size = 3;
    EXPECT_TRUE(wide.width() == 64 && wide.height() == 32);
    ObjAttrs tall = base_sprite();
    tall.shape = 2;
    tall.size = 1;
    EXPECT_TRUE(tall.width() == 8 && tall.height() == 32);
}

TEST(objmap, tile_addressing_1d_vs_2d) {
    ObjAttrs s = base_sprite();  // 16x16 -> two tiles per row in 1D
    s.bpp8 = false;
    // 1D: row 8 col 0 -> second tile row, first tile of that row.
    u32 off1d = sprite_pixel_byte(s, true, 0, 8);
    u32 off2d = sprite_pixel_byte(s, false, 0, 8);
    EXPECT_EQ(off1d, kObjTileBase + 2 * 32);        // 1D: third tile linearly
    EXPECT_EQ(off2d, kObjTileBase + 32 * 32);       // 2D: first tile of map row 1
}

TEST(objpixel, fetch_bank_and_flip) {
    PpuMemory m;
    ObjAttrs s = base_sprite();
    // Tile 0: byte 0 low nibble = 2.
    m.vram[kObjTileBase] = 0x02;
    int idx = obj_pixel_index(m, s, true, 0, 0);
    EXPECT_EQ(idx, int(kObjPalBase) + s.bank * 16 + 2);

    // Nibble 0 is transparent even with a bank set.
    m.vram[kObjTileBase] = 0x20;
    EXPECT_EQ(obj_pixel_index(m, s, true, 0, 0), -1);

    // hflip mirrors columns across the whole sprite: sampling column 14 of a
    // 16-wide sprite reads texture column 1.
    m.vram[kObjTileBase] = 0xF0;  // col0 -> 0 (transparent), col1 -> 15
    s.hflip = true;
    int flipped = obj_pixel_index(m, s, true, 14, 0);
    EXPECT_EQ(flipped, int(kObjPalBase) + 15);
}

TEST(affinesprite, rotation_by_identity_and_quarter_turn) {
    PpuMemory m;
    ObjAttrs s = base_sprite();  // 16x16 at local center (8,8)
    // Identity matrix keeps texels put.
    s16 pa = 256, pb = 0, pc = 0, pd = 256;
    int tx, ty;
    EXPECT_TRUE(affine_obj_texel(s, pa, pb, pc, pd, 8, 8, tx, ty));
    EXPECT_TRUE(tx == 8 && ty == 8);
    EXPECT_FALSE(affine_obj_texel(s, pa, pb, pc, pd, 40, 8, tx, ty));

    // Quarter turn: PA=0 PB=256 PC=-256 PD=0 rotates 90 degrees; screen
    // offset (+3,0) from center becomes texture offset (0,-3).
    load_obj_matrix(m, 0, pa, pb, pc, pd);  // zero matrix readback works
    pa = 0;
    pb = 256;
    pc = -256;
    pd = 0;
    EXPECT_TRUE(affine_obj_texel(s, pa, pb, pc, pd, 11, 8, tx, ty));
    EXPECT_TRUE(tx == 8 && ty == 5);
}

TEST(priority, sprite_wins_ties_bg_wins_lower_value) {
    // Equal priority: sprite wins.
    EXPECT_EQ(resolve_sprite_vs_bg(true, 1, 777, 1, 42), 777);
    // Background priority 0 beats sprite priority 1.
    EXPECT_EQ(resolve_sprite_vs_bg(true, 1, 777, 0, 42), 42);
    // Transparent sprite never occludes.
    EXPECT_EQ(resolve_sprite_vs_bg(false, 0, 777, 3, 42), 42);
}
