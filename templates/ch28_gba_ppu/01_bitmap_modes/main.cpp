#include <vector>
#define LABSTEST_MAIN
#include "labstest.hpp"
#include "bitmap.hpp"

using namespace gba;

TEST(bgr555, extremes_map_exactly) {
    EXPECT_EQ(bgr555_to_rgba8888(0x0000), 0xFF000000u);  // black, opaque
    EXPECT_EQ(bgr555_to_rgba8888(0x7FFF), 0xFFFFFFFFu);  // white
}

TEST(bgr555, mid_value_replicates) {
    // 5-bit value 16 -> (16<<3)|(16>>2) = 128|4 = 132 in each channel.
    u32 px = bgr555_to_rgba8888(u16(16 | 16 << 5 | 16 << 10));
    EXPECT_EQ(px & 0xFF, 132u);
    EXPECT_EQ((px >> 8) & 0xFF, 132u);
    EXPECT_EQ((px >> 16) & 0xFF, 132u);
    EXPECT_EQ((px >> 24) & 0xFF, 255u);
}

TEST(memory, bus_decodes_regions) {
    PpuMemory m;
    m.wr16(0x05000002, 0x1234);
    EXPECT_EQ(m.rd16(0x05000002), 0x1234);
    m.wr16(0x06000100, 0xBEEF);
    EXPECT_EQ(m.rd16(0x06000100), 0xBEEF);
    // Unaligned bit is ignored like on hardware.
    m.wr16(0x06000101, 0x7777);
    EXPECT_EQ(m.rd16(0x06000100), 0x7777);
}

TEST(mode3, roundtrip_and_stride) {
    PpuMemory m;
    mode3_set_pixel(m, 10, 20, 0x7C00);  // full red in BGR555
    mode3_set_pixel(m, 11, 20, 0x03E0);
    EXPECT_EQ(mode3_get_pixel(m, 10, 20), 0x7C00);
    EXPECT_EQ(mode3_get_pixel(m, 11, 20), 0x03E0);
    // Row stride: writing row 21 must not disturb row 20.
    mode3_set_pixel(m, 10, 21, 0xFFFF);
    EXPECT_EQ(mode3_get_pixel(m, 10, 20), 0x7C00);
}

TEST(mode3, out_of_range_ignored) {
    PpuMemory m;
    mode3_set_pixel(m, -1, 0, 0xFFFF);
    mode3_set_pixel(m, 240, 0, 0xFFFF);
    mode3_set_pixel(m, 0, 160, 0xFFFF);
    EXPECT_EQ(mode3_get_pixel(m, 239, 159), 0);
}

TEST(mode4, palette_lookup_and_page_flip) {
    PpuMemory m;
    u32 pal_addr = PpuMemory::kPalBase + 5 * 2;
    m.wr16(pal_addr, 0x001F);  // entry 5 = red

    mode4_set_pixel(m, 3, 2, 5);
    EXPECT_EQ(mode4_get_pixel(m, 3, 2), 5);

    // Flip the display page; same coordinates now read the other buffer.
    u16 dc = m.dispcnt();
    m.wr16(PpuMemory::kIoBase, u16(dc | 0x10));
    EXPECT_EQ(mode4_get_pixel(m, 3, 2), 0);

    // Draw into the visible page and confirm the packed neighbor survives.
    mode4_set_pixel(m, 3, 2, 5);
    mode4_set_pixel(m, 4, 2, 9);
    EXPECT_EQ(mode4_get_pixel(m, 3, 2), 5);
    EXPECT_EQ(mode4_get_pixel(m, 4, 2), 9);
}

TEST(bitmap_info, per_mode_geometry) {
    BitmapModeInfo m3 = bitmap_mode_info(3);
    EXPECT_TRUE(m3.w == 240 && m3.h == 160 && !m3.paletted);
    BitmapModeInfo m4 = bitmap_mode_info(4);
    EXPECT_TRUE(m4.w == 240 && m4.h == 160 && m4.paletted);
    BitmapModeInfo m5 = bitmap_mode_info(5);
    EXPECT_TRUE(m5.w == 160 && m5.h == 128 && !m5.paletted);
    EXPECT_TRUE(forced_blank(0x40) && !forced_blank(0));
}

TEST(render_frame, mode3_pixels_land_in_buffer) {
    PpuMemory m;
    m.wr16(PpuMemory::kIoBase, 3);  // DISPCNT = mode 3
    for (int x = 0; x < kScreenW; ++x)
        mode3_set_pixel(m, x, 100, u16(x & 31));
    std::vector<u32> buf(kScreenW * kScreenH);
    render_bitmap_frame(m, buf.data());
    EXPECT_EQ(buf[100 * kScreenW + 7], bgr555_to_rgba8888(7));
}

TEST(render_frame, mode5_backdrop_outside_frame_and_determinism) {
    PpuMemory m;
    m.wr16(PpuMemory::kPalBase, 0x7C00);  // backdrop red
    m.wr16(PpuMemory::kIoBase, 5);        // mode 5, page 0
    m.wr16(PpuMemory::kVramBase, 0x03E0); // (0,0) green
    std::vector<u32> a(kScreenW * kScreenH), b(kScreenW * kScreenH);
    render_bitmap_frame(m, a.data());
    render_bitmap_frame(m, b.data());
    EXPECT_EQ(a[0], bgr555_to_rgba8888(0x03E0));       // inside frame
    EXPECT_EQ(a[200], bgr555_to_rgba8888(0x7C00));     // x=200 outside 160
    EXPECT_EQ(fnv64(a.data(), a.size() * 4),
              fnv64(b.data(), b.size() * 4));          // deterministic
}

