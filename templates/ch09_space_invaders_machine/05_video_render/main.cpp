#define LABSTEST_MAIN
#include "labstest.hpp"

#include "machine.hpp"

#include <cstdint>
#include <functional>
#include <vector>

// Renderer + machine composition tests. The orientation test is the
// heart: a VRAM pattern that distinguishes (col, y) from (y, col) catches
// the transposed-decode bug class immediately.

using namespace si;

namespace {

VramDevice vram_with(const std::function<void(uint8_t*)>& fill) {
    VramDevice v;
    // const_cast-free fill through a bus-style write path.
    uint8_t local[kVramSize] = {};
    fill(local);
    for (uint32_t i = 0; i < kVramSize; ++i)
        v.write(uint16_t(i), local[i]);
    return v;
}

}  // namespace

TEST(decode, byte_address_is_column_major) {
    uint8_t vram[kVramSize] = {};
    vram[17 * 32 + 0] = 0xAB;      // column 17, rows 0-7
    EXPECT_EQ(vram_byte(vram, 17, 3), 0xAB);
    EXPECT_EQ(vram_byte(vram, 17, 7), 0xAB);
    EXPECT_NE(vram_byte(vram, 3, 17), 0xAB);   // swapped args read elsewhere

    vram[200 * 32 + 31] = 0xCD;    // column 200, rows 248-255
    EXPECT_EQ(vram_byte(vram, 200, 255), 0xCD);
}

TEST(decode, bit_index_is_row_modulo_eight) {
    uint8_t vram[kVramSize] = {};
    vram[0] = 0x20;                // bit 5 only
    EXPECT_TRUE(vram_bit(vram, 0, 5));
    EXPECT_FALSE(vram_bit(vram, 0, 4));
    EXPECT_FALSE(vram_bit(vram, 0, 6));
}

TEST(render, single_lit_pixel_lands_at_upright_xy) {
    auto v = vram_with([](uint8_t* p) { p[100 * 32 + (50 / 8)] = uint8_t(1 << (50 % 8)); });
    Frame f;
    render_frame(v.bytes(), &f);
    const size_t off = (size_t(50) * kScreenWidth + 100) * 4;   // x=col, y=row
    EXPECT_EQ(f.rgba[off + 0], 0xFF);
    EXPECT_EQ(f.rgba[off + 1], 0xFF);
    EXPECT_EQ(f.rgba[off + 2], 0xFF);
    EXPECT_EQ(f.rgba[off + 3], 0xFF);
    // Neighbor pixels stay dark.
    EXPECT_EQ(f.rgba[off - 4], 0x00);
    EXPECT_EQ(f.rgba[off + kScreenWidth * 4], 0x00);
}

TEST(render, orientation_distinguishes_transpose) {
    // Light ONLY pixel (5, 90). A transposed decoder would light (90, 5).
    auto v = vram_with([](uint8_t* p) {
        p[5 * 32 + (90 / 8)] = uint8_t(1 << (90 % 8));
    });
    Frame f;
    render_frame(v.bytes(), &f);
    auto px = [&](int x, int y) {
        return f.rgba[(size_t(y) * kScreenWidth + x) * 4];
    };
    EXPECT_EQ(px(5, 90), 0xFF);
    EXPECT_EQ(px(90, 5), 0x00);
}

TEST(render, blank_vram_gives_uniform_black_opaque_frame) {
    VramDevice v;   // zeroed
    Frame f;
    render_frame(v.bytes(), &f);
    bool all_black = true;
    for (size_t i = 0; i < Frame::kBytes && all_black; i += 4)
        all_black = f.rgba[i] == 0 && f.rgba[i + 1] == 0 &&
                    f.rgba[i + 2] == 0 && f.rgba[i + 3] == 0xFF;
    EXPECT_TRUE(all_black);
}

TEST(hash, fnv64_matches_reference_values) {
    // FNV-1a 64 of "a" — published reference vector.
    const uint8_t a = 'a';
    EXPECT_EQ(fnv64(&a, 1), 0xAF63DC4C8601EC8Cull);
}

TEST(machine, runner_pipeline_produces_a_stable_hash) {
    SpaceInvadersMachine m;
    static const uint8_t rom[] = {0x76};   // immediate HALT: blank frame
    m.load_rom(rom, sizeof rom);
    m.run(kCyclesPerFrame, nullptr);
    const uint64_t h1 = m.frame_hash();
    m.run(kCyclesPerFrame, nullptr);
    EXPECT_EQ(m.frame_hash(), h1);         // deterministic across frames

    // Blank frame hash must differ from an all-white VRAM hash.
    VramDevice white;
    for (uint32_t i = 0; i < kVramSize; ++i)
        white.write(uint16_t(i), 0xFF);
    Frame wf;
    render_frame(white.bytes(), &wf);
    EXPECT_NE(h1, fnv64(wf.rgba, Frame::kBytes));
}
