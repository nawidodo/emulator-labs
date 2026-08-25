// Tests for exercise 01: tile decoding.
//
// The fixture tile is committed as
// tests/public/ch14_gameboy_ppu1/fixtures/tile_arrow.bin (16 bytes) and
// is duplicated here byte-for-byte so the unit suite stays
// self-contained.
#define LABSTEST_MAIN
#include <cstdio>

#include "labstest.hpp"
#include "tile.hpp"

namespace {

// 8x8 arrow: a rising diagonal (index 2 with an index-1 left neighbor)
// and a bottom bar mostly index 3.
const uint8_t kTile[16] = {
    0x02, 0x01,  // y0
    0x04, 0x02,  // y1
    0x08, 0x04,  // y2
    0x10, 0x08,  // y3
    0x20, 0x10,  // y4
    0x40, 0x20,  // y5
    0x7E, 0x3F,  // y6 bottom bar
    0x00, 0x00,  // y7 blank
};

}  // namespace

TEST(tile, planeBit_takes_msb_first) {
    EXPECT_EQ(gbtiles::planeBit(0x80, 0), 1);
    EXPECT_EQ(gbtiles::planeBit(0x80, 1), 0);
    EXPECT_EQ(gbtiles::planeBit(0x01, 7), 1);
    EXPECT_EQ(gbtiles::planeBit(0xA5, 1), 0);   // 1010_0101: bit6=0
    EXPECT_EQ(gbtiles::planeBit(0xA5, 2), 1);   // bit5=1
}

TEST(tile, pixel_combines_planes) {
    // Row 0: lo=0x02 hi=0x01 -> x6=(1,0)=1, x7=(0,1)=2.
    EXPECT_EQ(gbtiles::tilePixel(kTile, 6, 0), 1);
    EXPECT_EQ(gbtiles::tilePixel(kTile, 7, 0), 2);
    // Row 6: lo=0x7E hi=0x3F -> x1=(1,0)=1, x2..x6=(1,1)=3, x7=(0,1)=2.
    EXPECT_EQ(gbtiles::tilePixel(kTile, 0, 6), 0);
    EXPECT_EQ(gbtiles::tilePixel(kTile, 1, 6), 1);
    EXPECT_EQ(gbtiles::tilePixel(kTile, 3, 6), 3);
    EXPECT_EQ(gbtiles::tilePixel(kTile, 7, 6), 2);
}

TEST(tile, decode_matches_fixture_bytes) {
    uint8_t px[64];
    gbtiles::decodeTile(kTile, px);
    // Diagonal: for rows 0..5 the hi-plane-only pixel is at x=7-y and the
    // lo-plane-only neighbor at x=6-y.
    for (int y = 0; y < 6; ++y) {
        EXPECT_EQ(px[y * 8 + (7 - y)], 2);
        EXPECT_EQ(px[y * 8 + (6 - y)], 1);
    }
    EXPECT_EQ(px[6 * 8 + 4], 3);  // bottom bar interior
    EXPECT_EQ(px[7 * 8], 0);      // last row blank
}
