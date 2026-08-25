#define LABSTEST_MAIN
#include "labstest.hpp"

#include "shift_register.hpp"
#include "video.hpp"

// Both tests fail on the seeded skeleton and pass once fixed. Students
// must document bug / root cause / first divergence / fix / regression
// test in a bug-report.md next to this directory (see DEBUGGING.md).

namespace {

uint8_t ref_window(uint16_t v, unsigned amt) {
    amt &= 7;
    uint8_t out = 0;
    for (unsigned i = 0; i < 8; ++i)
        if ((v >> (amt + i)) & 1) out |= uint8_t(1u << i);
    return out;
}

}  // namespace

TEST(bug1, shift_amount_off_by_one_diverges_immediately) {
    // Canonical fixture values: raw 0xADDE, amount 3 -> window 0xBB.
    ShiftRegister sr;
    sr.write_data(0xDE);
    sr.write_data(0xAD);
    sr.set_amount(3);
    EXPECT_EQ(sr.read(), 0xBB);

    // Sweep against the independent bit-loop model: any shift-offset
    // error diverges within the first few (value, amount) pairs.
    for (uint32_t lo = 0; lo < 256; ++lo) {
        const uint16_t raw = uint16_t((uint32_t(lo ^ 0xFF) << 8) | lo);
        ShiftRegister s;
        s.write_data(uint8_t(lo));
        s.write_data(uint8_t(lo ^ 0xFF));
        for (unsigned amt = 0; amt < 8; ++amt) {
            s.set_amount(uint8_t(amt));
            if (s.read() != ref_window(raw, amt)) {
                EXPECT_EQ(s.read(), ref_window(raw, amt));
                return;   // first divergence found and reported
            }
        }
    }
}

TEST(bug1, top_amount_shifts_seven_not_zero) {
    // Raw 0x00FF at amount 7: the true window is (raw >> 7) & 0xFF =
    // 0x01 — NOT the low byte. An off-by-one that masks to
    // (amount+1)&7 == 0 would return 0xFF here instead.
    ShiftRegister sr;
    sr.write_data(0xFF);
    sr.write_data(0x00);
    sr.set_amount(7);
    EXPECT_EQ(sr.read(), 0x01);
}

TEST(bug2, renderer_is_upright_not_transposed) {
    // Light ONLY pixel (5, 90): a row/col-swapped decode lights (90, 5).
    uint8_t vram[si::kVramSize] = {};
    vram[5 * 32 + (90 / 8)] = uint8_t(1 << (90 % 8));

    si::Frame f;
    si::render_frame(vram, &f);
    auto px = [&](int x, int y) {
        return f.rgba[(size_t(y) * si::kScreenWidth + x) * 4];
    };
    EXPECT_EQ(px(5, 90), 0xFF);
    EXPECT_EQ(px(90, 5), 0x00);
}

TEST(bug2, gradient_hash_differs_from_transposed_render) {
    // A full-range gradient where byte k = k&0xFF, rendered upright,
    // must hash differently from the same bytes decoded with swapped
    // factors — real VRAM layouts are never transpose-symmetric.
    uint8_t vram[si::kVramSize] = {};
    for (uint32_t i = 0; i < si::kVramSize; ++i)
        vram[i] = uint8_t(i);

    si::Frame f;
    si::render_frame(vram, &f);
    const uint64_t upright = si::fnv64(f.rgba, si::Frame::kBytes);

    si::Frame g;   // hand-rolled transposed decode of the SAME bytes
    for (int col = 0; col < si::kScreenWidth; ++col)
        for (int y = 0; y < si::kScreenHeight; ++y) {
            const bool on =
                ((vram[(size_t(y >> 3) * 32 + col)] >> (y & 7)) & 1) != 0;
            uint8_t* px = &g.rgba[(size_t(y) * si::kScreenWidth + col) * 4];
            px[0] = px[1] = px[2] = on ? 0xFF : 0x00;
            px[3] = 0xFF;
        }
    EXPECT_NE(upright, si::fnv64(g.rgba, si::Frame::kBytes));
}
