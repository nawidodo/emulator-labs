#define LABSTEST_MAIN
#include "labstest.hpp"
#include "chip8.hpp"

#include <vector>

namespace {

using chip8::Chip8;

Chip8 mk(const std::vector<uint8_t>& prog) {
    Chip8 c;
    c.reset();
    c.load(prog);
    return c;
}

void run(Chip8& c, int n) {
    for (int i = 0; i < n && !c.halted; ++i) c.step();
}

// 83X6 with X!=Y: source operand selection is the whole point.
const uint8_t kShr[] = {0x83, 0x46};   // SHR V3,V4

Chip8 shr(uint8_t v3, uint8_t v4, bool vy_source) {
    Chip8 c = mk(std::vector<uint8_t>(kShr, kShr + 2));
    c.v[3] = v3;
    c.v[4] = v4;
    c.quirks.shift_uses_vy = vy_source;
    run(c, 1);
    return c;
}

}  // namespace

// Modern profile: VX shifted in place, VY ignored entirely.
TEST(shift, right_modern_uses_vx) {
    Chip8 c = shr(0x81, 0xFF, false);
    EXPECT_EQ(c.v[3], 0x40);
    EXPECT_EQ(c.v[0xF], 1);      // old bit0 of VX, not VY (VY bit0 = 1 too!)
}

// Distinguish the sources: pick operands whose bit0 DIFFER.
TEST(shift, right_flag_source_follows_quirk) {
    Chip8 modern = shr(0x82, 0x81, false);  // VX bit0=0, VY bit0=1
    EXPECT_EQ(modern.v[3], 0x41);
    EXPECT_EQ(modern.v[0xF], 0);            // from VX

    Chip8 cosmac = shr(0x82, 0x81, true);   // same program, other profile
    EXPECT_EQ(cosmac.v[3], 0x40);           // from VY: 0x81>>1
    EXPECT_EQ(cosmac.v[0xF], 1);
}

// COSMAC profile: VY supplies the value AND the flag bit.
TEST(shift, right_cosmac_uses_vy) {
    Chip8 c = shr(0x00, 0x0F, true);
    EXPECT_EQ(c.v[3], 0x07);
    EXPECT_EQ(c.v[0xF], 1);
}

// Left shift mirrors the rule with bit 7.
TEST(shift, left_both_profiles) {
    const uint8_t shl[] = {0x85, 0x6E};   // SHL V5,V6
    Chip8 modern = mk(std::vector<uint8_t>(shl, shl + 2));
    modern.v[5] = 0xC0; modern.v[6] = 0x01;
    modern.quirks.shift_uses_vy = false;
    run(modern, 1);
    EXPECT_EQ(modern.v[5], 0x80);         // 0xC0<<1 truncated
    EXPECT_EQ(modern.v[0xF], 1);

    Chip8 cosmac = mk(std::vector<uint8_t>(shl, shl + 2));
    cosmac.v[5] = 0xC0; cosmac.v[6] = 0x01;
    cosmac.quirks.shift_uses_vy = true;
    run(cosmac, 1);
    EXPECT_EQ(cosmac.v[5], 0x02);         // 0x01<<1
    EXPECT_EQ(cosmac.v[0xF], 0);
}

// When X == Y both profiles must agree - the classic self-shift idiom that
// lets quirk-agnostic programs work everywhere.
TEST(shift, x_equals_y_is_profile_neutral) {
    const uint8_t shr_self[] = {0x87, 0x76};  // SHR V7,V7
    Chip8 a = mk(std::vector<uint8_t>(shr_self, shr_self + 2));
    a.v[7] = 0x9A;
    run(a, 1);
    Chip8 b = mk(std::vector<uint8_t>(shr_self, shr_self + 2));
    b.v[7] = 0x9A;
    b.quirks.shift_uses_vy = true;
    run(b, 1);
    EXPECT_EQ(a.v[7], 0x4D);
    EXPECT_EQ(b.v[7], 0x4D);
    EXPECT_EQ(a.v[0xF], b.v[0xF]);
}

// The full ALU still works around the shifts (regression guard).
TEST(shift, alu_still_intact_around_shifts) {
    Chip8 c = mk({0x61, 0xFF,
                  0x62, 0x10,
                  0x81, 0x22,   // AND V1,V2 -> 0x10
                  0x81, 0x23}); // XOR V1,V2 -> 0x00
    run(c, 4);
    EXPECT_EQ(c.v[1], 0x00);
}
