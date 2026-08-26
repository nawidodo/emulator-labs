#define LABSTEST_MAIN
#include "labstest.hpp"
#include <memory>

#include <cstdint>

#include "../shared/vram.hpp"
#include "raster.hpp"

using namespace psx::gpu;

namespace {

DrawConfig full_area() {
    DrawConfig c{};
    c.area_x1 = 0;
    c.area_y1 = 0;
    c.area_x2 = kVramWidth - 1;
    c.area_y2 = kVramHeight - 1;
    return c;
}

}  // namespace

// --- BUG(1): the drawing area is INCLUSIVE on both ends -----------------

TEST(debug41, inclusive_clip_keeps_last_row_and_column) {
    auto v_storage = std::make_unique<Vram>();
    Vram& v = *v_storage;
    DrawConfig c = full_area();
    c.area_x1 = 10; c.area_y1 = 10;
    c.area_x2 = 20; c.area_y2 = 20;

    write_pixel(v, c, 20, 20, 0x00FF00);   // exact corner must land
    EXPECT_EQ(v.at(20, 20), rgb888_to_bgr555(0x00FF00));
    write_pixel(v, c, 21, 20, 0x00FF00);   // one past must not
    EXPECT_EQ(v.at(21, 20), 0);
}

TEST(debug41, mask_bits_behave) {
    auto v_storage = std::make_unique<Vram>();
    Vram& v = *v_storage;
    DrawConfig c = full_area();
    c.check_mask = true;
    v.at(5, 5) = 0x8000 | 123;             // write-protected destination
    write_pixel(v, c, 5, 5, 0xFFFFFF);
    EXPECT_EQ(v.at(5, 5) & 0x7FFF, 123);   // untouched

    DrawConfig s = full_area();
    s.set_mask = true;
    write_pixel(v, s, 6, 6, 0x101010);
    EXPECT_TRUE((v.at(6, 6) & 0x8000) != 0);
}

// --- BUG(2): rectangle height uses the HEIGHT normalization table -------

TEST(debug41, zero_height_degenerates_to_full_height) {
    // GP0(60h) with h=0: hardware draws 512 rows (the height table max).
    auto v_storage = std::make_unique<Vram>();
    Vram& v = *v_storage;
    DrawConfig c = full_area();
    draw_rectangle(v, c, 0x808080, 100, 200, /*w*/4, /*h*/0);
    EXPECT_EQ(v.at(100, kVramHeight - 1), rgb888_to_bgr555(0x808080));
}

TEST(debug41, rectangle_stamps_rows_by_height_not_width) {
    // A 4-wide x 2-tall rect must paint 4 columns across 2 rows (and h=0
    // degenerates to the height-table max of 512).
    auto v_storage = std::make_unique<Vram>();
    Vram& v = *v_storage;
    DrawConfig c = full_area();
    draw_rectangle(v, c, 0xF0F0F0, 50, 50, /*w*/4, /*h*/2);
    EXPECT_TRUE(v.at(53, 51) != 0);   // far corner of the true shape
    EXPECT_EQ(v.at(51, 54), 0);       // would be lit if loops transposed
}

TEST(debug41, drawing_offset_applies_to_rects) {
    auto v_storage = std::make_unique<Vram>();
    Vram& v = *v_storage;
    DrawConfig c = full_area();
    c.off_x = 30;
    c.off_y = -20;
    draw_rectangle(v, c, 0xF0F0F0, 10, 40, 2, 2);   // lands at (40,20)
    EXPECT_EQ(v.at(40, 20), rgb888_to_bgr555(0xF0F0F0));
}
