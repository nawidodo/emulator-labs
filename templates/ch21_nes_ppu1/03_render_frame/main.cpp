#define LABSTEST_MAIN
#include "labstest.hpp"

#include <array>
#include <cstdint>

#include "render_fixture.hpp"

using namespace nes21fix;
using namespace nes21render;

namespace {

Snapshot blank_scene() {
    Snapshot s;
    s.mirroring = 0;   // horizontal
    s.ctrl = 0x00;     // pattern table 0, nametable 0
    s.pal[0x00] = 0x0F;
    return s;
}

}  // namespace

TEST(nes21fix, nesf_layout_size) {
    EXPECT_EQ(kNesfSize, size_t(10542));
}

TEST(nes21fix, parse_roundtrip) {
    std::vector<uint8_t> blob(kNesfSize, 0);
    blob[0] = 'N'; blob[1] = 'E'; blob[2] = 'S'; blob[3] = 'F';
    blob[4] = 1;
    blob[5] = 1;                       // vertical mirroring
    blob[6] = 0x10;                    // ctrl: bg from pattern table 1
    blob[9] = 5;                       // fine X (unused by ch21 renderer)
    blob[14 + 0x1234] = 0xAB;
    Snapshot out;
    std::string err;
    EXPECT_TRUE(parse_nesf(blob, out, err));
    EXPECT_EQ(out.mirroring, 1);
    EXPECT_EQ(out.ctrl, 0x10);
    EXPECT_EQ(out.fine_x, 5);
    EXPECT_EQ(out.chr[0x1234], 0xAB);
    EXPECT_TRUE(err.empty());
}

TEST(nes21fix, parse_rejects_bad_magic_and_version) {
    Snapshot out;
    std::string err;
    std::vector<uint8_t> blob(kNesfSize, 0);
    EXPECT_FALSE(parse_nesf(blob, out, err));
    blob[0] = 'N'; blob[1] = 'E'; blob[2] = 'S'; blob[3] = 'F';
    blob[4] = 9;
    EXPECT_FALSE(parse_nesf(blob, out, err));
}

TEST(nes21render, blank_frame_is_uniform_backdrop) {
    Snapshot s = blank_scene();
    std::array<uint8_t, 256 * 240 * 4> frame{};
    render_snapshot_frame(frame, s);
    // Backdrop palette entry $0F -> master palette black.
    for (size_t i = 0; i < frame.size(); i += 4) {
        EXPECT_EQ(frame[i], 0);
        EXPECT_EQ(frame[i + 1], 0);
        EXPECT_EQ(frame[i + 2], 0);
        if (frame[i] != 0) break;
    }
    EXPECT_EQ(frame[3], 0xFF);
}

TEST(nes21render, tile_writes_show_through_mirroring_window) {
    Snapshot s = blank_scene();
    s.mirroring = 1;  // vertical: physical page selected by bit 10.
    s.nt[0x0000] = 1;  // logical NT A, top-left tile
    s.nt[0x0400] = 2;  // logical NT B, top-left tile
    for (int y = 0; y < 8; ++y) {
        s.chr[1 * 16 + y] = 0xF0;
        s.chr[2 * 16 + y] = 0x00;
        s.chr[1 * 16 + 8 + y] = 0x0F;
        s.chr[2 * 16 + 8 + y] = 0xFF;
    }
    s.pal[0x01] = 0x11;
    s.pal[0x02] = 0x16;
    s.pal[0x05] = 0x21;

    std::array<uint8_t, 256 * 240 * 4> frame{};
    render_snapshot_frame(frame, s);
    // Renderer resolves the $2000 window through PpuBus mirroring: vertical
    // Tile 1 rows: low=$F0 (left half), high=$0F (right half).
    // Pixel (4,0): planes 0/1 -> color 2 -> pal[$02]=$16.
    // Pixel (0,7): planes 1/0 -> color 1 -> pal[$01]=$11.
    size_t px_right = (size_t(0) * 256 + 4) * 4;
    EXPECT_EQ(frame[px_right + 0], 152);   // $16 = (152, 34, 32)
    EXPECT_EQ(frame[px_right + 1], 34);
    size_t px_left = (size_t(7) * 256 + 0) * 4;
    EXPECT_EQ(frame[px_left + 0], 8);      // $11 = (8, 76, 196)
    EXPECT_EQ(frame[px_left + 1], 76);
    EXPECT_EQ(frame[px_left + 2], 196);
}
