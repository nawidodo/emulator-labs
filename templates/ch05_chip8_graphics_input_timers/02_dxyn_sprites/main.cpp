#define LABSTEST_MAIN
#include "labstest.hpp"
#include "gfx.hpp"

#include <array>

namespace {

int lit_count(const chip8::Display& d) {
    int n = 0;
    for (int y = 0; y < chip8::kHeight; ++y)
        for (int x = 0; x < chip8::kWidth; ++x)
            if (d.pixels[y * chip8::kWidth + x]) ++n;
    return n;
}

}  // namespace

TEST(dxyn, single_pixel_sprite) {
    chip8::Display d;
    chip8::Chip8Quirks q;
    const std::array<uint8_t, 1> sprite{0x80};  // one pixel at column 0
    EXPECT_FALSE(chip8::draw_sprite(d, sprite.data(), 1, 10, 20, q));
    EXPECT_TRUE(d.get(10, 20));
    EXPECT_EQ(lit_count(d), 1);
}

TEST(dxyn, full_row_sprite) {
    chip8::Display d;
    chip8::Chip8Quirks q;
    const std::array<uint8_t, 2> sprite{0xFF, 0xFF};  // 2 rows of 8
    EXPECT_FALSE(chip8::draw_sprite(d, sprite.data(), 2, 5, 5, q));
    for (int row = 0; row < 2; ++row)
        for (int bit = 0; bit < 8; ++bit)
            EXPECT_TRUE(d.get(5 + bit, 5 + row));
    EXPECT_EQ(lit_count(d), 16);
}

TEST(dxyn, zero_bits_in_row_draw_nothing) {
    chip8::Display d;
    chip8::Chip8Quirks q;
    const std::array<uint8_t, 3> sprite{0x00, 0x00, 0x00};
    EXPECT_FALSE(chip8::draw_sprite(d, sprite.data(), 3, 0, 0, q));
    EXPECT_EQ(lit_count(d), 0);
}

TEST(dxyn, xor_erases_and_reports_collision) {
    chip8::Display d;
    chip8::Chip8Quirks q;
    const uint8_t bits = 0xFF;
    // First draw lights the row.
    EXPECT_FALSE(chip8::draw_sprite(d, &bits, 1, 0, 0, q));
    // Second draw over the same spot erases it and sets collision.
    EXPECT_TRUE(chip8::draw_sprite(d, &bits, 1, 0, 0, q));
    EXPECT_FALSE(d.get(0, 0));
    // Third draw re-lights: no collision again.
    EXPECT_FALSE(chip8::draw_sprite(d, &bits, 1, 0, 0, q));
}

TEST(dxyn, partial_overlap_collides_only_on_overlap) {
    chip8::Display d;
    chip8::Chip8Quirks q;
    const std::array<uint8_t, 1> sprite{0xF0};
    EXPECT_FALSE(chip8::draw_sprite(d, sprite.data(), 1, 4, 4, q));  // lights cols 4-7
    // Overlapping draw at cols 6-9: cols 6-7 collide, 8-9 are new.
    EXPECT_TRUE(chip8::draw_sprite(d, sprite.data(), 1, 6, 4, q));
    EXPECT_TRUE(d.get(4, 4));   // outside second sprite, still lit
    EXPECT_TRUE(d.get(5, 4));   // outside second sprite, still lit
    EXPECT_FALSE(d.get(6, 4));  // collided -> erased
    EXPECT_FALSE(d.get(7, 4));  // collided -> erased
    EXPECT_TRUE(d.get(8, 4));   // newly lit
    EXPECT_TRUE(d.get(9, 4));   // newly lit
}

TEST(dxyn, clip_right_edge_keeps_visible_part) {
    chip8::Display d;
    chip8::Chip8Quirks q;  // wrapping = false -> clip
    const std::array<uint8_t, 1> sprite{0xF0};  // cols 0-3 of the sprite lit
    // Origin x=62: sprite columns would land on 62,63,64,65. Only 62,63 fit.
    EXPECT_FALSE(chip8::draw_sprite(d, sprite.data(), 1, 62, 0, q));
    EXPECT_TRUE(d.get(62, 0));
    EXPECT_TRUE(d.get(63, 0));
    EXPECT_EQ(lit_count(d), 2);
}

TEST(dxyn, clip_bottom_edge_keeps_visible_part) {
    chip8::Display d;
    chip8::Chip8Quirks q;
    const std::array<uint8_t, 4> rows{0xFF, 0xFF, 0xFF, 0xFF};
    // Origin y=30: rows 30 and 31 fit; rows 32,33 are clipped.
    EXPECT_FALSE(chip8::draw_sprite(d, rows.data(), 4, 0, 30, q));
    EXPECT_TRUE(d.get(0, 30));
    EXPECT_TRUE(d.get(0, 31));
    EXPECT_EQ(lit_count(d), 16);
}

TEST(dxyn, clip_corner_full_offscreen_draws_nothing) {
    chip8::Display d;
    chip8::Chip8Quirks q;
    const std::array<uint8_t, 2> rows{0xFF, 0xFF};
    EXPECT_FALSE(chip8::draw_sprite(d, rows.data(), 2, 60, 40, q));
    EXPECT_EQ(lit_count(d), 0);
}

TEST(dxyn, wrap_x_reenters_left_edge) {
    chip8::Display d;
    chip8::Chip8Quirks q;
    q.wrapping = true;
    const std::array<uint8_t, 1> sprite{0xF0};  // sprite cols 0-3
    // Origin x=62: pixels land on 62,63,64->0,65->1.
    EXPECT_FALSE(chip8::draw_sprite(d, sprite.data(), 1, 62, 0, q));
    EXPECT_TRUE(d.get(62, 0));
    EXPECT_TRUE(d.get(63, 0));
    EXPECT_TRUE(d.get(0, 0));
    EXPECT_TRUE(d.get(1, 0));
    EXPECT_EQ(lit_count(d), 4);
}

TEST(dxyn, wrap_y_reenters_top_edge) {
    chip8::Display d;
    chip8::Chip8Quirks q;
    q.wrapping = true;
    const std::array<uint8_t, 4> rows{0x80, 0x80, 0x80, 0x80};
    // Origin y=30: rows land on 30,31,32->0,33->1.
    EXPECT_FALSE(chip8::draw_sprite(d, rows.data(), 4, 0, 30, q));
    EXPECT_TRUE(d.get(0, 30));
    EXPECT_TRUE(d.get(0, 31));
    EXPECT_TRUE(d.get(0, 0));
    EXPECT_TRUE(d.get(0, 1));
    EXPECT_EQ(lit_count(d), 4);
}

TEST(dxyn, wrap_collision_across_seam) {
    chip8::Display d;
    chip8::Chip8Quirks q;
    q.wrapping = true;
    const std::array<uint8_t, 1> sprite{0xC0};  // sprite cols 0-1
    // Pre-light both landing spots for origin_x=63: sprite col 0 hits
    // screen column 63, sprite col 1 wraps onto screen column 0.
    d.set(0, 5, true);
    d.set(63, 5, true);
    EXPECT_TRUE(chip8::draw_sprite(d, sprite.data(), 1, 63, 5, q));
    EXPECT_FALSE(d.get(0, 5));   // erased via wrap seam
    EXPECT_FALSE(d.get(63, 5));  // erased by direct hit
}

TEST(dxyn, large_origin_wraps_not_clips_when_quirk_set) {
    chip8::Display d;
    chip8::Chip8Quirks q;
    q.wrapping = true;
    const std::array<uint8_t, 1> sprite{0x80};
    // x=66 wraps to 2, y=70 wraps to 6.
    EXPECT_FALSE(chip8::draw_sprite(d, sprite.data(), 1, 66, 70, q));
    EXPECT_TRUE(d.get(2, 6));
}

TEST(dxyn, zero_height_sprite_is_noop) {
    chip8::Display d;
    chip8::Chip8Quirks q;
    const uint8_t bits = 0xFF;
    EXPECT_FALSE(chip8::draw_sprite(d, &bits, 0, 0, 0, q));
    EXPECT_EQ(lit_count(d), 0);
}
