#define LABSTEST_MAIN
#include "labstest.hpp"

#include "cpu.hpp"

using namespace nes6502;

namespace {

struct Rig {
    FlatRam ram;
    Cpu cpu{.bus = &ram};

    void exec(std::initializer_list<uint8_t> bytes) {
        cpu.load_program(0x0200, bytes.begin(), bytes.size());
        step(cpu);
    }
};

}  // namespace

TEST(compare, cmp_sets_carry_when_reg_ge) {
    Rig r;
    r.exec({0xA9, 0x42});
    r.exec({0xC9, 0x42});  // CMP #imm: equal -> C and Z
    EXPECT_TRUE(r.cpu.p & FC);
    EXPECT_TRUE(r.cpu.p & FZ);
    Rig r2;
    r2.exec({0xA9, 0x41});
    r2.exec({0xC9, 0x42});  // less -> no carry, N set from $FF
    EXPECT_FALSE(r2.cpu.p & FC);
    EXPECT_TRUE(r2.cpu.p & FN);
}

TEST(compare, cpx_cpy_use_their_registers) {
    Rig r;
    r.cpu.x = 0x10;
    r.cpu.y = 0x20;
    r.exec({0xE4, 0x30});  // CPX zp (mem[0x30]=0)
    EXPECT_TRUE(r.cpu.p & FC);
    r.exec({0xC4, 0x30});  // CPY zp
    EXPECT_TRUE(r.cpu.p & FC);
}

TEST(inc_dec, inc_zp_is_five_cycles_with_dummy_write) {
    Rig r;
    r.ram.mem[0x40] = 0xFF;
    r.exec({0xE6, 0x40});
    EXPECT_EQ(r.ram.mem[0x40], 0x00);
    EXPECT_TRUE(r.cpu.p & FZ);
    EXPECT_EQ(r.cpu.cycles, 5);  // opcode+operand+read+dummy write+write

    Rig r2;
    r2.exec({0xE8});  // INX from 0
    EXPECT_EQ(r2.cpu.x, 1);
    EXPECT_FALSE(r2.cpu.p & FZ);
    Rig r3;
    r3.cpu.x = 5;
    r3.exec({0xCA});  // DEX
    EXPECT_EQ(r3.cpu.x, 4);
}

TEST(shifts, asl_moves_bit7_into_carry) {
    Rig r;
    r.exec({0xA9, 0x80});
    r.exec({0x0A});  // ASL A
    EXPECT_EQ(r.cpu.a, 0x00);
    EXPECT_TRUE(r.cpu.p & FC);
    EXPECT_TRUE(r.cpu.p & FZ);

    Rig r2;
    r2.ram.mem[0x50] = 0b0100'0001;
    r2.exec({0x46, 0x50});  // LSR zp: bit0 -> C
    EXPECT_EQ(r2.ram.mem[0x50], 0b0010'0000);
    EXPECT_TRUE(r2.cpu.p & FC);
    EXPECT_EQ(r2.cpu.cycles, 5);
}

TEST(shifts, rol_ror_rotate_through_carry) {
    Rig r;
    r.exec({0x38});          // SEC
    r.exec({0xA9, 0x00});
    r.exec({0x2A});          // ROL A: carry in -> bit0
    Rig r2;
    r2.exec({0xA9, 0x01});
    r2.exec({0x6A});         // ROR A: bit0 -> carry, carry-in was 0
    EXPECT_EQ(r2.cpu.a, 0x00);
    EXPECT_TRUE(r2.cpu.p & FC);
}

TEST(branch, not_taken_costs_two_cycles) {
    Rig r;
    r.exec({0xA9, 0x01});    // LDA #1 clears Z
    const uint64_t before = r.cpu.cycles;
    r.cpu.pc = 0x0202;
    r.ram.mem[0x0202] = 0xF0;  // BEQ
    r.ram.mem[0x0203] = 0x10;
    step(r.cpu);
    EXPECT_EQ(r.cpu.cycles - before, 2);
    EXPECT_EQ(r.cpu.pc, 0x0204);
}

TEST(branch, taken_same_page_three_cycles) {
    Rig r;
    r.ram.mem[0x0200] = 0xD0;  // BNE
    r.ram.mem[0x0201] = 0x05;
    r.cpu.pc = 0x0200;
    r.cpu.bus = &r.ram;
    step(r.cpu);
    EXPECT_EQ(r.cpu.cycles, 3);
    EXPECT_EQ(r.cpu.pc, 0x0207);
}

TEST(branch, taken_page_cross_four_cycles) {
    Rig r;
    // Branch at $20FD, operand at $20FE: the next-fetch PC is $20FF and
    // offset +$05 lands at $2104 - a different page than $20.
    r.ram.mem[0x20FD] = 0xD0;
    r.ram.mem[0x20FE] = 0x05;
    r.cpu.pc = 0x20FD;
    r.cpu.bus = &r.ram;
    step(r.cpu);
    EXPECT_EQ(r.cpu.cycles, 4);
    EXPECT_EQ(r.cpu.pc, 0x2104);
}

TEST(jumps, jmp_indirect_uses_page_wrap_quirk) {
    Rig r;
    r.ram.mem[0x30FF] = 0x11;
    r.ram.mem[0x3000] = 0x22;
    r.ram.mem[0x0200] = 0x6C;  // JMP ($30FF)
    r.ram.mem[0x0201] = 0xFF;
    r.ram.mem[0x0202] = 0x30;
    r.cpu.pc = 0x0200;
    r.cpu.bus = &r.ram;
    step(r.cpu);
    EXPECT_EQ(r.cpu.pc, 0x2211);
    EXPECT_EQ(r.cpu.cycles, 5);
}

TEST(subroutines, jsr_rts_round_trip) {
    Rig r;
    // JSR $0300 ; JAM marker after the call returns
    r.ram.mem[0x0200] = 0x20;
    r.ram.mem[0x0201] = 0x00;
    r.ram.mem[0x0202] = 0x03;
    r.ram.mem[0x0203] = 0x02;  // unimplemented opcode halts cleanly
    r.ram.mem[0x0300] = 0xEA;  // NOP
    r.ram.mem[0x0301] = 0x60;  // RTS
    r.cpu.pc = 0x0200;
    r.cpu.bus = &r.ram;
    run(r.cpu, 10);
    EXPECT_TRUE(r.cpu.halted);
    EXPECT_EQ(r.cpu.pc, 0x0204);
    EXPECT_EQ(r.cpu.sp, 0xFD);  // stack balanced
    EXPECT_EQ(r.cpu.cycles, 6 + 2 + 6 + 1);
}

TEST(subroutines, brk_pushes_b_and_vectors_through_fffe) {
    Rig r;
    r.ram.mem[0x0200] = 0x00;  // BRK
    r.cpu.pc = 0x0200;
    r.cpu.bus = &r.ram;
    r.ram.mem[0xFFFE] = 0x34;
    r.ram.mem[0xFFFF] = 0x12;
    const uint16_t ret = 0x0202;  // BRK addr + 2
    step(r.cpu);
    EXPECT_EQ(r.cpu.pc, 0x1234);
    EXPECT_TRUE(r.cpu.p & FI);
    // Stack: [PCH][PCL][P|B|U]
    EXPECT_EQ(r.ram.mem[0x01FD], ret >> 8);
    EXPECT_EQ(r.ram.mem[0x01FC], uint8_t(ret & 0xFF));
    EXPECT_EQ(r.ram.mem[0x01FB], uint8_t(FU | FB | FI));
}

TEST(flags, setters_clearers) {
    Rig r;
    r.exec({0x38});  // SEC
    EXPECT_TRUE(r.cpu.p & FC);
    r.exec({0x18});  // CLC
    EXPECT_FALSE(r.cpu.p & FC);
    r.exec({0xB8});  // CLV always works even though V was clear
    EXPECT_FALSE(r.cpu.p & FV);
}
