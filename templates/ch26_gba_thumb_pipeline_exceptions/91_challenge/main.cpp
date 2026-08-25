#define LABSTEST_MAIN
#include "labstest.hpp"
#include "challenge_cpu.hpp"

using namespace challenge;
using thumb::kF3MOV;

static constexpr uint16_t f3(uint32_t op, uint32_t rd, uint32_t imm8) {
    return static_cast<uint16_t>(0x2000u | (op << 11) | (rd << 8) | imm8);
}
static constexpr uint16_t push(bool lr, uint32_t list) {
    return static_cast<uint16_t>(0xB400u | ((lr ? 1u : 0u) << 8) | list);
}
static constexpr uint16_t pop(bool pc, uint32_t list) {
    return static_cast<uint16_t>(0xBC00u | ((pc ? 1u : 0u) << 8) | list);
}
static constexpr uint16_t bany(int32_t byte_off) {
    return static_cast<uint16_t>(0xE000u | ((byte_off >> 1) & 0x7FF));
}

TEST(challenge, push_stores_ascending_with_writeback) {
    ChallengeCpu cpu;
    cpu.r[13] = 0x800;
    cpu.r[1] = 11;
    cpu.r[4] = 44;
    cpu.write16(0x00, push(false, (1u << 1) | (1u << 4)));   // PUSH {r1,r4}
    cpu.step();
    EXPECT_EQ(cpu.read32(0x7F8), 11u);      // lowest register, lowest addr
    EXPECT_EQ(cpu.read32(0x7FC), 44u);
    EXPECT_EQ(cpu.r[13], 0x7F8u);           // full-descending writeback
}

TEST(challenge, pop_loads_and_post_increments) {
    ChallengeCpu cpu;
    cpu.write32(0x900, 0xAAu);
    cpu.write32(0x904, 0xBBu);
    cpu.r[13] = 0x900;
    cpu.write16(0x00, pop(false, (1u << 2) | (1u << 7)));   // POP {r2,r7}
    cpu.step();
    EXPECT_EQ(cpu.r[2], 0xAAu);
    EXPECT_EQ(cpu.r[7], 0xBBu);
    EXPECT_EQ(cpu.r[13], 0x908u);
}

TEST(challenge, push_lr_pop_pc_implements_call_return) {
    ChallengeCpu cpu;
    // Callee at 0x20: PUSH {LR}; clobber; POP {PC} returns to caller.
    cpu.write16(0x20, push(true, 0));                        // PUSH {lr}
    cpu.write16(0x22, f3(kF3MOV, 0, 0));                     // clobber r0
    cpu.write16(0x24, pop(true, 0));                         // POP {pc}
    cpu.r[14] = 0x48;                                        // return address
    cpu.r[15] = 0x20;
    cpu.step();                                              // PUSH {lr}
    EXPECT_EQ(cpu.read32(cpu.r[13]), 0x48u);
    cpu.step();                                              // clobber
    EXPECT_EQ(cpu.r[0], 0u);
    const unsigned cyc = cpu.step();                         // POP {pc}
    EXPECT_TRUE(cyc >= 3u);                                  // refill paid
    EXPECT_EQ(cpu.r[15], 0x48u);
    EXPECT_TRUE(cpu.t);                                      // T unchanged
}

TEST(challenge, nested_push_pop_preserves_caller_state) {
    ChallengeCpu cpu;
    cpu.r[13] = 0x1000;
    cpu.r[0] = 5;
    cpu.r[1] = 9;
    cpu.write16(0x40, push(false, 0x3));                     // PUSH {r0,r1}
    cpu.write16(0x42, f3(kF3MOV, 0, 0));                     // r0 = 0
    cpu.write16(0x44, f3(kF3MOV, 1, 0));                     // r1 = 0
    cpu.write16(0x46, pop(false, 0x3));                      // POP {r0,r1}
    for (int i = 0; i < 4; ++i) cpu.step();
    EXPECT_EQ(cpu.r[0], 5u);
    EXPECT_EQ(cpu.r[1], 9u);
    EXPECT_EQ(cpu.r[13], 0x1000u);
}

TEST(hidden, challenge_hidden_stack_semantics) {
    ChallengeCpu cpu;
    // PUSH {r0-r3, LR} then POP {r0-r3, PC}: five words each way.
    cpu.r[13] = 0x2000;
    for (int i = 0; i < 4; ++i)
        cpu.r[i] = static_cast<uint32_t>(0x10 + i);
    cpu.r[14] = 0x300;
    cpu.write16(0x00, push(true, 0x0F));                     // + LR bit8
    cpu.step();
    EXPECT_EQ(cpu.r[13], 0x2000u - 20);
    EXPECT_EQ(cpu.read32(0x2000 - 20), 0x10u);               // r0 lowest
    EXPECT_EQ(cpu.read32(0x2000 - 4), 0x300u);               // LR last
    for (int i = 0; i < 4; ++i) cpu.r[i] = 0;
    cpu.write16(0x02, pop(true, 0x0F));                      // + PC bit8
    cpu.step();
    for (int i = 0; i < 4; ++i)
        EXPECT_EQ(cpu.r[i], static_cast<uint32_t>(0x10 + i));
    EXPECT_EQ(cpu.r[15], 0x300u);
    EXPECT_EQ(cpu.r[13], 0x2000u);
}
