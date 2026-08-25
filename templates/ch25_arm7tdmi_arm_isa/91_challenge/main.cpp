#define LABSTEST_MAIN
#include "labstest.hpp"
#include "challenge_ldm.hpp"

using namespace arm;

// cond 100 P U S W L Rn reglist
static constexpr uint32_t block(bool pre, bool up, bool wb, bool load,
                                uint32_t rn, uint32_t list) {
    return 0xE0000000u | (0x8 << 24) | (pre ? 1u << 24 : 0) |
           (up ? 1u << 23 : 0) | (wb ? 1u << 21 : 0) |
           (load ? 1u << 20 : 0) | (rn << 16) | list;
}

TEST(challenge, reg_count_popcounts) {
    EXPECT_EQ(reg_count(0x0000), 0u);
    EXPECT_EQ(reg_count(0x6003), 4u);   // r0,r1,r14,r15
    EXPECT_EQ(reg_count(0xFFFF), 16u);
}

TEST(challenge, stmia_writes_ascending) {
    ArmCpu cpu;
    cpu.r[13] = 0x1000;
    cpu.r[1] = 11;
    cpu.r[2] = 22;
    cpu.r[4] = 44;
    exec_block(cpu, block(false, true, true, false, 13, 0x0016));  // STMIA r13!,{r1,r2,r4}
    EXPECT_EQ(cpu.read32(0x1000), 11u);
    EXPECT_EQ(cpu.read32(0x1004), 22u);
    EXPECT_EQ(cpu.read32(0x1008), 44u);
    EXPECT_EQ(cpu.r[13], 0x100Cu);                                 // writeback
}

TEST(challenge, stmdb_behaves_like_push) {
    ArmCpu cpu;
    cpu.r[13] = 0x2000;
    cpu.r[5] = 55;
    // STMDB r13!, {r5} behaves like a push: base ends below the stored word.
    exec_block(cpu, block(true, false, true, false, 13, 0x0020));
    EXPECT_EQ(cpu.read32(0x2000 - 4), 55u);
    EXPECT_EQ(cpu.r[13], 0x2000u - 4);
}

TEST(challenge, ldmia_restores_and_writeback) {
    ArmCpu cpu;
    cpu.write32(0x3000, 0xAA);
    cpu.write32(0x3004, 0xBB);
    cpu.r[9] = 0x3000;
    exec_block(cpu, block(false, true, false, true, 9, 0x0060));  // LDMIA r9,{r5,r6}
    EXPECT_EQ(cpu.r[5], 0xAAu);
    EXPECT_EQ(cpu.r[6], 0xBBu);
    EXPECT_EQ(cpu.r[9], 0x3000u);      // no W bit: base untouched
    exec_block(cpu, block(false, true, true, true, 9, 0x0060));   // with !
    EXPECT_EQ(cpu.r[9], 0x3008u);
}

TEST(hidden, challenge_hidden_block_semantics) {
    ChallengeCpu cpu;
    // Push {r0-r3} via STMDB sp!, pop them back.
    cpu.r[0] = 1; cpu.r[1] = 2; cpu.r[2] = 3; cpu.r[3] = 4;
    cpu.r[13] = 0x4000;
    cpu.write32(0x000, block(true, false, true, false, 13, 0x000F));  // push
    cpu.write32(0x004, block(false, true, true, true, 13, 0x000F));   // pop
    cpu.step();
    EXPECT_EQ(cpu.r[13], 0x4000u - 16);
    EXPECT_EQ(cpu.read32(0x4000 - 16), 1u);
    EXPECT_EQ(cpu.read32(0x4000 - 4), 4u);
    cpu.step();
    EXPECT_EQ(cpu.r[13], 0x4000u);
    EXPECT_EQ(cpu.r[0], 1u);
    EXPECT_EQ(cpu.r[3], 4u);
}
