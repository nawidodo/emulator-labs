#define LABSTEST_MAIN
#include "labstest.hpp"
#include <cstddef>

#include <cstring>
#include <string>

#include "chip8.hpp"
#include "quirks.hpp"

namespace {

using ch06::Chip8;
using ch06::Chip8Quirks;

struct FinalState {
    uint8_t v[16];
    uint16_t i;
    uint16_t pc;
    uint8_t vf() const { return v[0xF]; }
};

FinalState run(const uint8_t* rom, size_t len, const std::string& profile) {
    const auto q = ch06::profile_by_name(profile);
    EXPECT_TRUE(q.has_value());
    Chip8 cpu;
    cpu.reset(*q);
    cpu.load({rom, len});
    int guard = 0;
    while (!cpu.halted() && guard++ < 1000) cpu.step();
    FinalState f;
    std::memcpy(f.v, cpu.v, 16);
    f.i = cpu.i;
    f.pc = cpu.pc;
    return f;
}

// Q1: 8XY6 shift — VIP/CHIP48 shift VY into VX, MODERN shifts VX.
const uint8_t kQ1Shift[] = {
    0x62, 0xF0,  // 200: LD  V2, 0xF0
    0x63, 0x01,  // 202: LD  V3, 0x01
    0x82, 0x36,  // 204: SHR V2, {V3|V2}
    0x84, 0x20,  // 206: LD  V4, V2
};
// Q2: FX55/FX65 — VIP leaves I alone so the reload sees the saved data.
const uint8_t kQ2LoadStore[] = {
    0x63, 0x12,  // 200: LD  V3, 0x12
    0xA4, 0x00,  // 202: LD  I, 0x400
    0xF3, 0x55,  // 204: LD  [I], V3
    0xF3, 0x65,  // 206: LD  V3, [I]
};
// Q3: BXNN — CHIP48 adds VX (V2 here), everyone else adds V0 (=05).
const uint8_t kQ3Jump[] = {
    0x60, 0x05,  // 200: LD  V0, 0x05
    0x62, 0x03,  // 202: LD  V2, 0x03
    0xB2, 0x05,  // 204: JP  205 + {V0|V2}
    0x00, 0x00,  // 206: NOP (padding)
    0x12, 0x14,  // 208: JP  214      <- CHIP48 lands here (205+V2)
    0x64, 0x11,  // 20A: LD  V4, 0x11 <- MODERN lands here (205+V0)
    0x12, 0x14,  // 20C: JP  214
    0x00, 0x00,  // 20E: NOP (padding)
};
// Q4: DXYN wrapping — a sprite drawn at x=62 wraps onto columns 0..5 under
// VIP and collides with pixels drawn there earlier; clipped profiles don't.
const uint8_t kQ4DrawWrap[] = {
    0x60, 0xAA,  // 200: LD   V0, 0xAA \ sprite row bytes
    0x61, 0xAA,  // 202: LD   V1, 0xAA /
    0xA4, 0x00,  // 204: LD   I, 0x400
    0xF1, 0x55,  // 206: LD   [I], V1 -> mem[400]=mem[401]=AA
    0x60, 0x00,  // 208: LD   V0, 0
    0x61, 0x00,  // 20A: LD   V1, 0
    0xA4, 0x00,  // 20C: LD   I, 0x400
    0xD0, 0x12,  // 20E: DRW  V0, V1, 2   -> cols 0..7 rows 0..1 set
    0x60, 0x3E,  // 210: LD   V0, 62
    0xA4, 0x00,  // 212: LD   I, 0x400
    0xD0, 0x12,  // 214: DRW  V0, V1, 2   -> wrap hits cols 0..1: VF=1
};
// Q5: vf_reset — FX1E clears VF before adding under MODERN only.
const uint8_t kQ5VfReset[] = {
    0x64, 0xFF,  // 200: LD   V4, 0xFF
    0xA4, 0x00,  // 202: LD   I, 0x400
    0xF4, 0x1E,  // 204: ADD  I, V4
    0x65, 0xFF,  // 206: LD   V5, 0xFF
    0x66, 0x11,  // 208: LD   V6, 0x11
    0x86, 0x44,  // 20A: ADD  V6, V4   -> carry, VF=1
    0xA4, 0x00,  // 20C: LD   I, 0x400
    0xF4, 0x1E,  // 20E: ADD  I, V4   -> VF reset to 0 under MODERN
};

}  // namespace

TEST(profiles, known_and_unknown_names) {
    EXPECT_TRUE(ch06::profile_by_name("MODERN").has_value());
    EXPECT_TRUE(ch06::profile_by_name("COSMAC_VIP").has_value());
    EXPECT_TRUE(ch06::profile_by_name("CHIP48").has_value());
    EXPECT_FALSE(ch06::profile_by_name("POTATO").has_value());

    const auto vip = ch06::profile_by_name("COSMAC_VIP");
    EXPECT_EQ(vip->shift_uses_vy, true);
    EXPECT_EQ(vip->load_store_leaves_i, true);
    EXPECT_EQ(vip->wrapping, true);
    EXPECT_EQ(vip->vf_reset, false);
    EXPECT_EQ(vip->jump_bnnn_x, false);

    const auto c48 = ch06::profile_by_name("CHIP48");
    EXPECT_EQ(c48->jump_bnnn_x, true);
    EXPECT_EQ(c48->load_store_leaves_i, false);
}

TEST(profiles, shift_quirk_differs) {
    const auto vip = run(kQ1Shift, sizeof kQ1Shift, "COSMAC_VIP");
    const auto mod = run(kQ1Shift, sizeof kQ1Shift, "MODERN");
    EXPECT_EQ(vip.v[2], 0x00);  // V2 = V3 >> 1
    EXPECT_EQ(vip.v[4], 0x00);
    EXPECT_EQ(vip.vf(), 0x01);
    EXPECT_EQ(mod.v[2], 0x78);  // V2 = V2 >> 1
    EXPECT_EQ(mod.v[4], 0x78);
    EXPECT_EQ(mod.vf(), 0x00);
    EXPECT_NE(vip.v[2], mod.v[2]);
}

TEST(profiles, loadstore_quirk_differs) {
    const auto vip = run(kQ2LoadStore, sizeof kQ2LoadStore, "COSMAC_VIP");
    const auto mod = run(kQ2LoadStore, sizeof kQ2LoadStore, "MODERN");
    EXPECT_EQ(vip.v[3], 0x12);  // reload restores the stored value
    EXPECT_EQ(vip.i, 0x400);    // ...and I stayed put
    EXPECT_EQ(mod.v[3], 0x00);  // reload reads past the save area
    EXPECT_EQ(mod.i, 0x408);
    EXPECT_NE(vip.v[3], mod.v[3]);
}

TEST(profiles, jump_quirk_differs) {
    const auto c48 = run(kQ3Jump, sizeof kQ3Jump, "CHIP48");
    const auto mod = run(kQ3Jump, sizeof kQ3Jump, "MODERN");
    EXPECT_EQ(c48.v[4], 0x00);  // jumped over the V4 load via VX indexing
    EXPECT_EQ(mod.v[4], 0x11);
}

TEST(profiles, draw_wrap_quirk_differs) {
    const auto vip = run(kQ4DrawWrap, sizeof kQ4DrawWrap, "COSMAC_VIP");
    const auto mod = run(kQ4DrawWrap, sizeof kQ4DrawWrap, "MODERN");
    EXPECT_EQ(vip.vf(), 0x01);  // wrapped sprite collided with itself
    EXPECT_EQ(mod.vf(), 0x00);  // clipped off-screen: no collision
}

TEST(profiles, vf_reset_quirk_differs) {
    const auto mod = run(kQ5VfReset, sizeof kQ5VfReset, "MODERN");
    const auto vip = run(kQ5VfReset, sizeof kQ5VfReset, "COSMAC_VIP");
    EXPECT_EQ(mod.vf(), 0x00);
    EXPECT_EQ(vip.vf(), 0x01);
}
