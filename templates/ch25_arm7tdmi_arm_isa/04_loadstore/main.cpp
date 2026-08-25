#define LABSTEST_MAIN
#include "labstest.hpp"
#include "loadstore.hpp"

using namespace arm;

// Encoding helpers mirroring the ARM ARM A4-4/A4-6 layouts.
static constexpr uint32_t ls_imm(bool pre, bool up, bool byte, bool wb,
                                 bool load, uint32_t rn, uint32_t rd,
                                 uint32_t imm12) {
    return 0xE4000000u | (pre ? 1u << 24 : 0) | (up ? 1u << 23 : 0) |
           (byte ? 1u << 22 : 0) | (wb ? 1u << 21 : 0) |
           (load ? 1u << 20 : 0) | (rn << 16) | (rd << 12) | imm12;
}
static constexpr uint32_t ls_reg(bool pre, bool up, bool byte, bool wb,
                                 bool load, uint32_t rn, uint32_t rd,
                                 uint32_t rm) {
    return ls_imm(pre, up, byte, wb, load, rn, rd, 0) | (1u << 25) | rm;
}

TEST(loadstore, str_then_ldr_roundtrip) {
    LoadStoreCpu cpu;
    cpu.r[1] = 0x1000;
    cpu.r[2] = 0xDEADBEEFu;
    cpu.exec_ls(ls_imm(true, true, false, false, false, 1, 2, 0));  // STR r2,[r1]
    EXPECT_EQ(cpu.r[1], 0x1000u);                                   // no writeback
    cpu.exec_ls(ls_imm(true, true, false, false, true, 1, 3, 0));   // LDR r3,[r1]
    EXPECT_EQ(cpu.r[3], 0xDEADBEEFu);
}

TEST(loadstore, little_endian_layout) {
    LoadStoreCpu cpu;
    cpu.r[1] = 0x2000;
    cpu.r[2] = 0x11223344u;
    cpu.exec_ls(ls_imm(true, true, false, false, false, 1, 2, 0));
    EXPECT_EQ(cpu.mem[0x2000], 0x44);
    EXPECT_EQ(cpu.mem[0x2001], 0x33);
    EXPECT_EQ(cpu.mem[0x2002], 0x22);
    EXPECT_EQ(cpu.mem[0x2003], 0x11);
}

TEST(loadstore, post_index_writeback) {
    LoadStoreCpu cpu;
    cpu.r[1] = 0x3000;
    cpu.r[2] = 42;
    // STR r2,[r1],#4 -> stores at 0x3000 then r1 += 4.
    cpu.exec_ls(ls_imm(false, true, false, false, false, 1, 2, 4));
    EXPECT_EQ(cpu.r[1], 0x3004u);
    // LDRB r3,[r1],#-4 -> reads at 0x3004, then r1 -= 4.
    cpu.mem[0x3004] = 0xAB;
    cpu.exec_ls(ls_imm(false, false, true, false, true, 1, 3, 4));
    EXPECT_EQ(cpu.r[1], 0x3000u);
    EXPECT_EQ(cpu.r[3], 0xABu);
}

TEST(loadstore, pre_index_writeback) {
    LoadStoreCpu cpu;
    cpu.r[1] = 0x4000;
    cpu.r[2] = 7;
    // STR r2,[r1,#8]! -> stores at 0x4008 and writes back.
    cpu.exec_ls(ls_imm(true, true, false, true, false, 1, 2, 8));
    EXPECT_EQ(cpu.r[1], 0x4008u);
    EXPECT_EQ(cpu.load_word(0x4008), 7u);
}

TEST(loadstore, register_offset_and_directions) {
    LoadStoreCpu cpu;
    cpu.r[1] = 0x5000;
    cpu.r[2] = 0x10;
    cpu.r[3] = 0x55;
    cpu.exec_ls(ls_reg(true, true, true, false, false, 1, 3, 2));  // STRB r3,[r1,r2]
    EXPECT_EQ(cpu.load_byte(0x5010), 0x55);
    cpu.store_word(0x5000 - 0x10, 0xCAFEBABEu);
    cpu.exec_ls(ls_reg(true, false, false, false, true, 1, 4, 2)); // LDR r4,[r1,-r2]
    EXPECT_EQ(cpu.load_byte(0x5000), 0);                           // untouched
    EXPECT_EQ(cpu.r[4], 0xCAFEBABEu);                              // from 0x4FF0
}

TEST(loadstore, unaligned_ldr_rotates) {
    LoadStoreCpu cpu;
    const uint32_t word = 0xAABBCCDDu;  // stored LE: DD CC BB AA
    cpu.store_word(0x6000, word);
    // Unaligned read at +1: bytes CC BB AA DD -> rotate right by 8 of the
    // aligned word gives the byte at the boundary in the MS position.
    cpu.r[1] = 0x6001;
    cpu.exec_ls(ls_imm(true, true, false, false, true, 1, 5, 0));
    EXPECT_EQ(cpu.r[5], 0xDDAABBCCu);
}

TEST(loadstore, cycles_are_1n_1s) {
    LoadStoreCpu cpu;
    EXPECT_EQ(cpu.exec_ls(ls_imm(true, true, false, false, true, 0, 0, 0)), 2u);
}

TEST(hidden, loadstore_hidden_semantics) {
    LoadStoreCpu cpu;
    // Byte store keeps only the low 8 bits.
    cpu.r[1] = 0x7000;
    cpu.r[2] = 0xFFFFFF12u;
    cpu.exec_ls(ls_imm(true, true, true, false, false, 1, 2, 0));
    EXPECT_EQ(cpu.load_byte(0x7000), 0x12);
    // Word writeback with subtract offset.
    cpu.r[3] = 0x7010;
    cpu.exec_ls(ls_imm(true, false, false, true, false, 3, 2, 0x10));  // STR r2,[r3,#-16]!
    EXPECT_EQ(cpu.r[3], 0x7000u);
    EXPECT_EQ(cpu.load_word(0x7000), 0xFFFFFF12u);
}
