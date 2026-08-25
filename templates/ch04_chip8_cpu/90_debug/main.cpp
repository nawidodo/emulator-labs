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

}  // namespace

// Each test below isolates ONE seeded flag bug. Write them up in
// bug-report.md using the format required by DEBUGGING.md:
// bug / root cause / first divergence / fix / regression test.

// BUG 1: 8XY6/8XYE read the shifted-out bit from the wrong source register.
TEST(bug_hunt, bug1_shift_vf_source) {
    const uint8_t shr[] = {0x83, 0x46};  // SHR V3,V4
    Chip8 c = mk(std::vector<uint8_t>(shr, shr + 2));
    c.v[3] = 0x81;            // bit0 of VX is 1 ...
    c.v[4] = 0x80;            // ... but bit0 of VY is 0
    c.quirks.shift_uses_vy = false;      // modern profile: VX is the source
    run(c, 1);
    EXPECT_EQ(c.v[3], 0x40);
    EXPECT_EQ(c.v[0xF], 1);   // FAILS while the bug is present

    // Left shift carries the same defect.
    const uint8_t shl[] = {0x85, 0x6E};   // SHL V5,V6
    Chip8 d = mk(std::vector<uint8_t>(shl, shl + 2));
    d.v[5] = 0x85;            // old bit7 of VX is 1
    d.v[6] = 0x01;            // bit7 of VY is 0
    d.quirks.shift_uses_vy = false;
    run(d, 1);
    EXPECT_EQ(d.v[5], 0x0A);
    EXPECT_EQ(d.v[0xF], 1);
}

// BUG 2: 8XY7 borrow direction inverted (copies 8XY5's comparison).
TEST(bug_hunt, bug2_subn_borrow_direction) {
    const uint8_t subn[] = {0x80, 0xE7};  // SUBN V0,VE  (VX = VE - VX)
    Chip8 c = mk(std::vector<uint8_t>(subn, subn + 2));
    c.v[0] = 5;               // VX > VE -> borrow happened -> VF must be 0
    c.v[0xE] = 3;
    run(c, 1);
    EXPECT_EQ(c.v[0], 0xFE);
    EXPECT_EQ(c.v[0xF], 0);   // FAILS while the bug is present
}

// BUG 3: 4XNN skips when it should not (behaves like 3XNN).
TEST(bug_hunt, bug3_sne_inverted) {
    Chip8 c = mk({0x61, 0x07,
                  0x41, 0x07,   // SNE V1,7 with V1==7: condition FALSE, no skip
                  0x62, 0xAA}); // ...so this MUST execute
    run(c, 3);
    EXPECT_EQ(c.v[2], 0xAA);  // FAILS while the bug is present

    // The equal case must still skip (3XNN unaffected).
    Chip8 d = mk({0x61, 0x07,
                  0x31, 0x07,
                  0x62, 0xAA,
                  0x63, 0xBB});
    run(d, 3);
    EXPECT_EQ(d.v[2], 0);
    EXPECT_EQ(d.v[3], 0xBB);
}

// BUG 4: FX33 stores hundreds and ones in swapped slots.
TEST(bug_hunt, bug4_bcd_digit_order) {
    Chip8 c = mk({0xA4, 0x00, 0xF0, 0x33});
    c.v[0] = 205;             // BCD 2-0-5, never a palindrome
    run(c, 2);
    EXPECT_EQ(c.mem[0x400], 2);   // hundreds
    EXPECT_EQ(c.mem[0x401], 0);   // tens
    EXPECT_EQ(c.mem[0x402], 5);   // ones -- FAILS while the bug is present
}

// End-to-end guard: one small program touching all four repaired families.
TEST(bug_hunt, integration_after_all_four_fixes) {
    Chip8 c = mk({0x61, 0xD1,          // V1 = D1 (shift source below)
                  0x62, 0xCD,          // V2 = 205 (BCD input)
                  0x63, 0xFF,
                  0x83, 0x16,          // SHR V3 <- VY (cosmac profile)
                  0xA4, 0x00,          // I = 0x400
                  0xF2, 0x33,          // BCD V2 -> 2 0 5
                  0x71, 0x2F});        // V1 += 2F wraps to 00 silently
    c.quirks.shift_uses_vy = true;
    run(c, 7);
    EXPECT_EQ(c.v[3], 0x68);           // D1 >> 1 via the VY source path
    EXPECT_EQ(c.v[0xF], 1);            // shifted-out bit of VY
    EXPECT_EQ(c.mem[0x400], 2);        // BCD hundreds
    EXPECT_EQ(c.mem[0x401], 0);
    EXPECT_EQ(c.mem[0x402], 5);        // ones back in their slot
    EXPECT_EQ(c.v[1], 0x00);           // imm add wrapped without flags
}
