#define LABSTEST_MAIN
#include "labstest.hpp"
#include "switch_cpu.hpp"

using namespace dual;

static constexpr uint16_t mov_high(uint32_t rd, uint32_t rm) {
    // F5 MOV rd, rm with H bits folded in (works for low/high regs).
    return static_cast<uint16_t>(0x4600u | (((rm >> 3) & 1) << 6) |
                                 (((rd >> 3) & 1) << 7) |
                                 ((rm & 7) << 3) | (rd & 7));
}
static constexpr uint16_t adds_imm(uint32_t rd, uint32_t imm8) {
    return static_cast<uint16_t>(0x3000u | (rd << 8) | imm8);
}
static constexpr uint16_t f3(uint32_t op, uint32_t rd, uint32_t imm8) {
    return static_cast<uint16_t>(0x2000u | (op << 11) | (rd << 8) | imm8);
}
static constexpr uint16_t bany(int32_t byte_off) {
    return static_cast<uint16_t>(0xE000u | ((byte_off >> 1) & 0x7FF));
}

// ARM encoders (words).
static constexpr uint32_t arm_mov_imm(uint32_t rd, uint32_t rot,
                                      uint32_t imm8) {
    return 0xE3A00000u | (rd << 12) | (rot << 8) | imm8;
}
static constexpr uint32_t arm_add_imm(uint32_t rn, uint32_t rd, uint32_t imm) {
    return 0xE2800000u | (rn << 16) | (rd << 12) | imm;
}
static constexpr uint32_t arm_subs_reg(uint32_t rn, uint32_t rd, uint32_t rm) {
    return 0xE0500000u | (rn << 16) | (rd << 12) | rm;
}
static constexpr uint32_t arm_movw_lr(uint32_t imm) {
    return arm_mov_imm(14, 0, imm);
}
static constexpr uint32_t arm_bx(uint32_t rm) {
    return 0xE12FFF10u | rm;
}
static constexpr uint32_t arm_b_self() {
    return 0xEAFFFFFEu;
}
static constexpr uint32_t arm_bl(uint32_t at, uint32_t target) {
    return 0xEB000000u | (((target - (at + 8)) >> 2) & 0xFFFFFF);
}

TEST(switch, bx_sets_state_from_bit0) {
    SwitchCpu c2;
    c2.do_bx(0x41);
    EXPECT_TRUE(c2.t);
    EXPECT_EQ(c2.r[15], 0x40u);
    c2.do_bx(0x40);                      // even: back to ARM
    EXPECT_FALSE(c2.t);
    EXPECT_EQ(c2.r[15], 0x40u);
}

TEST(switch, arm_bl_links_to_next_slot) {
    SwitchCpu cpu;
    cpu.write32(0x00, arm_bl(0, 0x10));  // BL to 0x10
    cpu.write32(0x10, arm_b_self());
    cpu.step();                          // BL: LR = 0 + 4
    EXPECT_EQ(cpu.r[14], 0x04u);
    EXPECT_EQ(cpu.r[15], 0x10u);
    EXPECT_FALSE(cpu.t);
}

TEST(switch, thumb_bl_links_past_pair) {
    SwitchCpu cpu;
    cpu.t = true;
    // BL pair at 0x80/0x82 targeting 0x88.
    const int32_t off = 0x88 - (0x80 + 4);           // = 4 -> imm11 = 2
    cpu.write16(0x80, static_cast<uint16_t>(0xF000 | ((off >> 12) & 0x7FF)));
    cpu.write16(0x82, static_cast<uint16_t>(0xF800 | ((off >> 1) & 0x7FF)));
    cpu.write16(0x88, bany(-4));                     // park
    cpu.r[15] = 0x80;
    cpu.step();                                      // first halfword
    cpu.step();                                      // second halfword: links
    EXPECT_EQ(cpu.r[14], 0x84u);                     // first_addr + 4
    EXPECT_EQ(cpu.r[15], 0x88u);
}

TEST(switch, interleave_round_trip) {
    SwitchCpu cpu;
    // ARM @0x00: r0=6+1, save resume in r12, BX into Thumb @0x41.
    cpu.write32(0x00, arm_mov_imm(0, 0, 6));
    cpu.write32(0x04, arm_add_imm(0, 0, 1));
    cpu.write32(0x08, arm_mov_imm(12, 0, 0x20));     // ARM resume address
    cpu.write32(0x0C, arm_mov_imm(3, 0, 0x41));      // Thumb entry | T
    cpu.write32(0x10, arm_bx(3));
    cpu.write32(0x20, arm_subs_reg(1, 2, 0));        // resumed: r2 = r1 - r0
    cpu.write32(0x24, arm_b_self());
    // Thumb @0x40: r1 = 9 + 8, BL inner (+1), then BX back to ARM via r12.
    cpu.write16(0x40, f3(thumb::kF3MOV, 1, 9));
    cpu.write16(0x42, adds_imm(1, 8));               // r1 = 17
    {
        const int32_t off = 0x4E - (0x44 + 4);       // BL inner @0x4E
        cpu.write16(0x44, static_cast<uint16_t>(0xF000 | ((off >> 12) & 0x7FF)));
        cpu.write16(0x46, static_cast<uint16_t>(0xF800 | ((off >> 1) & 0x7FF)));
    }
    cpu.write16(0x48, adds_imm(1, 1));               // r1 = 18 after return
    cpu.write16(0x4A, mov_high(3, 12));              // r3 = r12 = 0x20
    cpu.write16(0x4C, static_cast<uint16_t>(0x4718));  // BX r3 -> ARM @0x20
    cpu.write16(0x4E, adds_imm(0, 1));               // inner: r0 = 8
    cpu.write16(0x50, bany(-12));                    // B back to 0x48

    for (int i = 0; i < 20; ++i) cpu.step();
    EXPECT_EQ(cpu.r[0], 8u);
    EXPECT_EQ(cpu.r[1], 18u);
    EXPECT_EQ(cpu.r[2], 10u);                        // 18 - 8
    EXPECT_EQ(cpu.r[15], 0x24u);                     // parked in ARM
    EXPECT_FALSE(cpu.t);
}

TEST(hidden, switch_hidden_veneer_safety) {
    // The classic veneer: LR must hold an EVEN address so the final
    // BX lr lands back in ARM state at the instruction after the call.
    SwitchCpu cpu;
    cpu.write32(0x00, arm_mov_imm(3, 0, 0x51));      // thumb target | T
    cpu.write32(0x04, arm_movw_lr(0x10));            // LR = 0x10 (even)
    cpu.write32(0x08, arm_bx(3));
    cpu.write32(0x10, arm_b_self());
    cpu.write16(0x50, f3(thumb::kF3MOV, 4, 0x5A));
    for (int i = 0; i < 8; ++i) cpu.step();
    EXPECT_TRUE(cpu.t);                              // still in Thumb park?
    EXPECT_EQ(cpu.r[4], 0x5Au);
    EXPECT_EQ(cpu.r[14], 0x10u);                     // even link preserved
}
