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

// Ten opcode/mode combinations NOT covered by exercises 01-03. Each test
// checks semantics AND the official cycle count. Suite name "unseen" is
// used by the hidden grader.

TEST(unseen, ora_absx_1d) {
    Rig r;
    r.ram.mem[0x2100] = 0x0F;
    r.cpu.a = 0x30;
    r.cpu.x = 0x02;
    r.exec({0x1D, 0xFE, 0x20});  // ORA $20FE,X (X=2 -> $2100)
    EXPECT_EQ(r.cpu.a, 0x3F);
    EXPECT_EQ(r.cpu.cycles, 5);  // page cross penalty
}

TEST(unseen, and_absy_39) {
    Rig r;
    r.ram.mem[0x3010] = 0xFF;
    r.cpu.a = 0x0F;
    r.exec({0x39, 0x10, 0x30});  // AND $3010,Y with Y=0
    EXPECT_EQ(r.cpu.a, 0x0F);
    EXPECT_EQ(r.cpu.cycles, 4);
}

TEST(unseen, eor_izx_41) {
    Rig r;
    r.cpu.x = 0x10;              // operand $20 + X -> pointer $30
    r.ram.mem[0x0030] = 0x00;    // ptr lo
    r.ram.mem[0x0031] = 0x30;    // ptr hi -> $3000
    r.ram.mem[0x3000] = 0xC1;
    r.cpu.a = 0xF0;
    r.exec({0x41, 0x20});
    EXPECT_EQ(r.cpu.a, 0x31);
    EXPECT_EQ(r.cpu.cycles, 6);
}

TEST(unseen, adc_zpx_75) {
    Rig r;
    r.cpu.p |= FC;
    r.cpu.x = 0x05;
    r.ram.mem[0x0045] = 0x10;
    r.exec({0x75, 0x40});        // ADC $40,X
    EXPECT_EQ(r.cpu.a, 0x11);    // A(0) + $10 + carry-in
    EXPECT_EQ(r.cpu.cycles, 4);  // zp,X bills its index cycle always
}

TEST(unseen, sbc_absx_fd) {
    Rig r;
    r.cpu.p |= FC;               // no borrow
    r.ram.mem[0x2005] = 0x03;
    r.cpu.a = 0x05;
    r.cpu.x = 0x05;
    r.exec({0xFD, 0x00, 0x20});  // SBC $2000,X with X=5
    EXPECT_EQ(r.cpu.a, 0x02);
    EXPECT_EQ(r.cpu.cycles, 4);  // no crossing: base+X same page
}

TEST(unseen, cmp_izy_d1) {
    Rig r;
    r.ram.mem[0x40] = 0xFE;      // ptr lo
    r.ram.mem[0x41] = 0x21;      // ptr hi -> base $21FE
    r.ram.mem[0x2200] = 0x42;    // base + Y=2 -> $2200 (page crossed)
    r.cpu.y = 0x02;
    r.cpu.a = 0x42;
    r.exec({0xD1, 0x40});        // CMP ($40),Y crossing pages
    EXPECT_TRUE(r.cpu.p & FC);
    EXPECT_EQ(r.cpu.cycles, 6);  // 5 + cross
}

TEST(unseen, ror_zpx_76) {
    Rig r;
    r.cpu.p |= FC;               // carry in
    r.cpu.x = 0x70;
    r.ram.mem[0x0032] = 0x04;    // $C2,$X=$70 wraps to $32
    r.exec({0x76, 0xC2});
    EXPECT_EQ(r.ram.mem[0x0032], 0x82);   // carry-in rotated into bit 7
    EXPECT_FALSE(r.cpu.p & FC);           // old bit 0 was 0 -> carry out 0
    EXPECT_EQ(r.cpu.cycles, 6);           // zp,X always bills the index
}

TEST(unseen, inc_absx_fe_always_penalty) {
    Rig r;
    r.ram.mem[0x2000] = 0xFF;
    r.exec({0xFE, 0x00, 0x20});  // INC $2000,X with X=0
    EXPECT_EQ(r.ram.mem[0x2000], 0x00);
    EXPECT_TRUE(r.cpu.p & FZ);
    EXPECT_EQ(r.cpu.cycles, 7);  // Always-penalty row
}

TEST(unseen, dec_absx_de) {
    Rig r;
    r.ram.mem[0x300A] = 0x01;
    r.cpu.x = 0x0A;
    r.exec({0xDE, 0x00, 0x30});
    EXPECT_EQ(r.ram.mem[0x300A], 0x00);
    EXPECT_EQ(r.cpu.cycles, 7);
}

TEST(unseen, bit_zp_24) {
    Rig r;
    r.ram.mem[0x0070] = 0x41;    // bit 6 set only
    r.cpu.a = 0x41;
    r.exec({0x24, 0x70});
    EXPECT_FALSE(r.cpu.p & FN);
    EXPECT_TRUE(r.cpu.p & FV);
    EXPECT_FALSE(r.cpu.p & FZ);
    EXPECT_EQ(r.cpu.cycles, 3);
}
