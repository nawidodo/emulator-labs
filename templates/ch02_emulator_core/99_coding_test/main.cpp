#define LABSTEST_MAIN
#include "labstest.hpp"

#include <span>

#include "minicore.hpp"

namespace {

// Loads one 2-byte instruction at address 0 and executes it once against
// the given register state.
// Places one 2-byte instruction at address 0 WITHOUT resetting the machine,
// so tests can preset registers/flags first. (load() would wipe them.)
minicore::StepResult one(minicore::Cpu& c, uint8_t b0, uint8_t b1) {
    c.mem[0] = b0;
    c.mem[1] = b1;
    c.pc = 0;
    return c.step();
}

}  // namespace

TEST(minicore, ldi_loads_immediate) {
    minicore::Cpu c;
    const auto res = one(c, 0x12, 0xAB);   // LDI r2,#0xAB
    EXPECT_EQ(res.error, minicore::StepError::None);
    EXPECT_EQ(c.r[2], 0xAB);
    EXPECT_EQ(res.cycles, 2u);
    EXPECT_EQ(res.pc, 2);
    EXPECT_FALSE(c.zf);
    EXPECT_FALSE(c.cf);
}

TEST(minicore, mov_copies_between_registers) {
    minicore::Cpu c;
    c.r[3] = 0x5E;
    (void)one(c, 0x20, 0x30);              // MOV r0,r3
    EXPECT_EQ(c.r[0], 0x5E);
    EXPECT_EQ(c.r[3], 0x5E);
}

TEST(minicore, add_sets_carry_on_overflow) {
    minicore::Cpu c;
    c.r[1] = 200;
    c.r[2] = 100;
    (void)one(c, 0x31, 0x20);              // ADD r1,r2
    EXPECT_EQ(c.r[1], 44);
    EXPECT_TRUE(c.cf);
    EXPECT_FALSE(c.zf);
}

TEST(minicore, add_to_exact_zero_sets_both_flags) {
    minicore::Cpu c;
    c.r[0] = 128;
    c.r[3] = 128;
    (void)one(c, 0x30, 0x30);              // ADD r0,r3
    EXPECT_EQ(c.r[0], 0);
    EXPECT_TRUE(c.zf);
    EXPECT_TRUE(c.cf);
}

TEST(minicore, sub_borrow_flag_semantics) {
    minicore::Cpu c;
    c.r[0] = 3;
    c.r[1] = 10;
    (void)one(c, 0x40, 0x10);              // SUB r0,r1
    EXPECT_EQ(c.r[0], 249);
    EXPECT_TRUE(c.cf);                     // borrow
    EXPECT_FALSE(c.zf);

    minicore::Cpu d;
    d.r[0] = 10;
    d.r[1] = 3;
    (void)one(d, 0x40, 0x10);
    EXPECT_EQ(d.r[0], 7);
    EXPECT_FALSE(d.cf);
}

TEST(minicore, inc_updates_zero_never_carry) {
    minicore::Cpu c;
    c.r[0] = 0xFF;
    c.cf = true;
    (void)one(c, 0x50, 0x00);              // INC r0
    EXPECT_EQ(c.r[0], 0);
    EXPECT_TRUE(c.zf);
    EXPECT_TRUE(c.cf);                     // INC must NOT clear carry

    minicore::Cpu d;
    d.r[1] = 7;
    d.cf = true;
    (void)one(d, 0x51, 0x00);
    EXPECT_EQ(d.r[1], 8);
    EXPECT_FALSE(d.zf);
    EXPECT_TRUE(d.cf);                     // still untouched
}

TEST(minicore, dec_from_zero_sets_carry_and_wraps) {
    minicore::Cpu c;
    c.r[2] = 0x00;
    (void)one(c, 0x62, 0x00);              // DEC r2
    EXPECT_EQ(c.r[2], 0xFF);
    EXPECT_TRUE(c.cf);
    EXPECT_FALSE(c.zf);

    minicore::Cpu d;
    d.r[2] = 0x05;
    (void)one(d, 0x62, 0x00);
    EXPECT_EQ(d.r[2], 4);
    EXPECT_FALSE(d.cf);
}

TEST(minicore, shl_moves_bit7_into_carry) {
    minicore::Cpu c;
    c.r[1] = 0xC0;                         // 1100 0000
    (void)one(c, 0x71, 0x00);              // SHL r1
    EXPECT_EQ(c.r[1], 0x80);
    EXPECT_TRUE(c.cf);
    EXPECT_FALSE(c.zf);

    minicore::Cpu d;
    d.r[1] = 0x80;
    (void)one(d, 0x71, 0x00);
    EXPECT_EQ(d.r[1], 0);
    EXPECT_TRUE(d.cf);
    EXPECT_TRUE(d.zf);
}

TEST(minicore, ld_st_memory_roundtrip) {
    minicore::Cpu c;
    c.r[0] = 0x77;
    (void)one(c, 0xB0, 0xF0);              // ST r0,[0xF0]
    EXPECT_EQ(c.mem[0xF0], 0x77);
    EXPECT_EQ(c.pc, 2);

    minicore::Cpu d;
    d.mem[0x42] = 0x13;
    d.mem[0] = 0xA1;                       // LD r1,[0x42]
    d.mem[1] = 0x42;
    (void)d.step();
    EXPECT_EQ(d.r[1], 0x13);
    EXPECT_EQ(d.pc, 2);
}

TEST(minicore, jmp_ignores_the_x_nibble) {
    minicore::Cpu c;
    const auto res = one(c, 0x8F, 0x40);   // JMP 0x40 (x nibble is garbage)
    EXPECT_EQ(res.error, minicore::StepError::None);
    EXPECT_EQ(res.pc, 0x40);
    EXPECT_EQ(res.cycles, 2u);
}

TEST(minicore, jnz_asymmetric_cycle_costs) {
    minicore::Cpu d;
    d.zf = false;
    const auto taken = one(d, 0x90, 0x30); // JNZ 0x30, ZF=0 -> taken
    EXPECT_EQ(taken.pc, 0x30);
    EXPECT_EQ(taken.cycles, 2u);

    minicore::Cpu z;
    z.zf = true;
    const auto fall = one(z, 0x90, 0x30);  // ZF=1 -> fall through
    EXPECT_EQ(fall.pc, 2);
    EXPECT_EQ(fall.cycles, 1u);
}

TEST(minicore, st_ld_preserve_flags) {
    minicore::Cpu c;
    c.zf = true;
    c.cf = true;
    (void)one(c, 0xB2, 0x80);              // ST r2,[0x80]
    EXPECT_TRUE(c.zf);
    EXPECT_TRUE(c.cf);
}

TEST(minicore, hlt_costs_one_cycle) {
    minicore::Cpu c;
    const auto res = one(c, 0xD0, 0x00);
    EXPECT_TRUE(c.halted);
    EXPECT_EQ(res.cycles, 1u);
}

TEST(minicore, undefined_opcode_halts_deterministically) {
    minicore::Cpu c;
    const auto res = one(c, 0xC5, 0x99);   // 0xC is undefined
    EXPECT_EQ(res.error, minicore::StepError::UnknownOpcode);
    EXPECT_TRUE(c.halted);
    EXPECT_EQ(res.cycles, 1u);
    EXPECT_EQ(res.pc, 2);                  // both bytes consumed
    EXPECT_EQ(c.r[5 % 4], 0);              // nothing executed
}

TEST(minicore, reserved_register_field_is_an_error) {
    minicore::Cpu c;
    const auto res = one(c, 0x15, 0x01);   // LDI r5,... -> BadRegister
    EXPECT_EQ(res.error, minicore::StepError::BadRegister);
    EXPECT_TRUE(c.halted);
    EXPECT_EQ(res.cycles, 2u);

    minicore::Cpu d;
    const auto res2 = one(d, 0x21, 0x50);  // MOV r1,r5 -> BadRegister on y
    EXPECT_EQ(res2.error, minicore::StepError::BadRegister);

    minicore::Cpu e;
    const auto ok = one(e, 0x8A, 0x20);    // JMP ignores x even when > 3
    EXPECT_EQ(ok.error, minicore::StepError::None);
    EXPECT_EQ(ok.pc, 0x20);
}

TEST(minicore, countdown_loop_program) {
    // 0: LDI r0,#3 ; 2: DEC r0 ; 4: JNZ 2 ; 6: HLT
    const uint8_t prog[] = {0x10, 0x03, 0x60, 0x00, 0x90, 0x02, 0xD0, 0x00};
    minicore::Cpu c;
    c.load(std::span<const uint8_t>(prog, sizeof(prog)));
    const uint32_t spent = c.run(100);
    EXPECT_TRUE(c.halted);
    EXPECT_EQ(c.r[0], 0);
    // LDI(2) + 3 x DEC(2) + 2 x JNZ-taken(2) + JNZ-fallthrough(1) + HLT(1)
    EXPECT_EQ(spent, 14u);
    EXPECT_EQ(c.pc, 8);
}

TEST(minicore, memory_sum_program) {
    // mem[0x80]=25, mem[0x81]=17; sum them through registers into r3 and
    // store back: LD r0,[0x80]; LD r1,[0x81]; MOV r3,r0; ADD r3,r1; ST...
    minicore::Cpu c;
    const uint8_t prog[] = {0xA0, 0x80, 0xA1, 0x81,
                            0x23, 0x00, 0x33, 0x10,
                            0xB3, 0x82, 0xD0, 0x00};
    c.load(std::span<const uint8_t>(prog, sizeof(prog)));
    c.mem[0x80] = 25;
    c.mem[0x81] = 17;
    const uint32_t spent = c.run(100);
    EXPECT_TRUE(c.halted);
    EXPECT_EQ(c.mem[0x82], 42);
    EXPECT_EQ(spent, 3u * 2u + 2u * 2u + 3u + 1u);  // 2 LD + MOV + ADD + ST
}
