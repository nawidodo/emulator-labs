#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>

#include "sprite.hpp"

using namespace nes22sprite;

namespace {

// OAM with sprites at various rows.
void fill_oam(uint8_t* oam) {
    for (int i = 0; i < 256; ++i) oam[i] = 0;
    auto put = [&](int i, uint8_t y, uint8_t t, uint8_t a, uint8_t x) {
        oam[i * 4] = y; oam[i * 4 + 1] = t;
        oam[i * 4 + 2] = a; oam[i * 4 + 3] = x;
    };
    put(0, 10, 0x01, 0x00, 20);
    put(1, 30, 0x02, 0x00, 40);
}

}  // namespace

TEST(nes22sprite, evaluate_finds_in_range_sprites_in_order) {
    uint8_t oam[256];
    fill_oam(oam);
    bool ovf = false;
    EvalResult r = evaluate(oam, 12, false, ovf);   // y=10 covers lines 10-17
    EXPECT_EQ(r.count, 1);
    EXPECT_EQ(r.slots[0], 0);
    EXPECT_EQ(r.sprite0, true);
    EXPECT_EQ(ovf, false);

    EvalResult r2 = evaluate(oam, 31, false, ovf);  // only sprite 1
    EXPECT_EQ(r2.count, 1);
    EXPECT_EQ(r2.slots[0], 1);
    EXPECT_EQ(r2.sprite0, false);
}

TEST(nes22sprite, evaluate_range_boundaries_8_and_16) {
    uint8_t oam[256];
    fill_oam(oam);
    bool ovf = false;
    // 8px tall: line 18 is past sprite 0 (10+8).
    EXPECT_EQ(evaluate(oam, 18, false, ovf).count, 0);
    // 16px tall: line 18 still inside sprite 0 (10..25).
    EXPECT_EQ(evaluate(oam, 18, true, ovf).count, 1);
}

TEST(nes22sprite, evaluate_reports_overflow_past_eight) {
    uint8_t oam[256];
    for (int i = 0; i < 64; ++i) {
        oam[i * 4] = 10;      // all on the same line
        oam[i * 4 + 3] = uint8_t(i * 2);
    }
    bool ovf = false;
    EvalResult r = evaluate(oam, 11, false, ovf);
    EXPECT_EQ(r.count, 8);
    EXPECT_EQ(ovf, true);
    EXPECT_EQ(r.slots[7], 7);   // first eight by OAM order
}

TEST(nes22sprite, overflow_quirk_can_disagree_with_clean_model) {
    // First 8 sprites in range; entry 8's Y is off-screen but its TILE
    // byte is small enough that the misaligned counter reads it as a Y.
    uint8_t oam[256];
    for (int i = 0; i < 64; ++i) {
        oam[i * 4] = uint8_t(i < 8 ? 10 : 200);   // only first 8 in range
        oam[i * 4 + 1] = 0x40;                    // tile byte
    }
    oam[8 * 4 + 1] = 5;              // tile byte -> looks like y=5!
    bool clean = false;
    EvalResult r = evaluate(oam, 12, false, clean);
    EXPECT_EQ(r.count, 8);
    EXPECT_EQ(clean, false);         // clean model: no 9th in range
    // Quirk model compares entry 8's TILE byte (5) against line 12:
    // 12 is inside [5,13) -> false overflow.
    EXPECT_EQ(overflow_with_quirk(oam, 12, 8, false), true);
}

TEST(nes22sprite, overflow_quirk_agrees_when_bytes_align) {
    uint8_t oam[256];
    for (int i = 0; i < 64; ++i) oam[i * 4] = 10;
    bool clean = false;
    evaluate(oam, 12, false, clean);
    EXPECT_EQ(clean, true);
    EXPECT_EQ(overflow_with_quirk(oam, 12, 8, false), true);
}

namespace {
nes22sprite::MaskBits kAllVisible{false, false, true, true};
nes22sprite::MaskBits kBgClipped{true, false, true, true};
nes22sprite::MaskBits kBgOff{false, false, false, true};
nes22sprite::MaskBits kSprOff{false, false, true, false};
}  // namespace

TEST(nes22sprite, sprite0_hit_requires_both_opaque) {
    EXPECT_EQ(sprite0_hit_at(50, true, true, true, kAllVisible), true);
    EXPECT_EQ(sprite0_hit_at(50, false, true, true, kAllVisible), false);
    EXPECT_EQ(sprite0_hit_at(50, true, false, true, kAllVisible), false);
    EXPECT_EQ(sprite0_hit_at(50, true, true, false, kAllVisible), false);
}

TEST(nes22sprite, sprite0_hit_never_on_pixel_255) {
    EXPECT_EQ(sprite0_hit_at(255, true, true, true, kAllVisible), false);
    EXPECT_EQ(sprite0_hit_at(254, true, true, true, kAllVisible), true);
}

TEST(nes22sprite, sprite0_hit_left_column_clips) {
    EXPECT_EQ(sprite0_hit_at(7, true, true, true, kBgClipped), false);
    EXPECT_EQ(sprite0_hit_at(8, true, true, true, kBgClipped), true);
    nes22sprite::MaskBits spr_clip{false, true, true, true};
    EXPECT_EQ(sprite0_hit_at(3, true, true, true, spr_clip), false);
    EXPECT_EQ(sprite0_hit_at(9, true, true, true, spr_clip), true);
}

TEST(nes22sprite, sprite0_hit_requires_rendering_enabled) {
    EXPECT_EQ(sprite0_hit_at(50, true, true, true, kBgOff), false);
    EXPECT_EQ(sprite0_hit_at(50, true, true, true, kSprOff), false);
}
