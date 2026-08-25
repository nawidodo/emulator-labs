// Challenge fixture: straight-line 8080 program exercising the chapter 7
// arithmetic/logic matrix through every addressing mode available before
// control flow exists (chapter 8 adds jumps/calls).
//
// Hand-assembled listing (see also tests/public/ch07_i8080_architecture/):
//
//   addr  bytes        insn            effect
//   0000  3E 25        MVI A,25
//   0002  C6 41        ADI 41          A=66 CY=0 AC=0
//   0004  32 00 40     STA 4000        [4000]=66
//   0007  3E F0        MVI A,F0
//   0009  E6 3C        ANI 3C          A=30, AC=1 via 8080 AND quirk
//   000B  32 01 40     STA 4001        [4001]=30
//   000E  3E AA        MVI A,AA
//   0010  EE 55        XRI 55          A=FF CY=0 AC=0 S=1 P=1
//   0012  32 02 40     STA 4002        [4002]=FF
//   0015  3E 05        MVI A,05
//   0017  D6 07        SUI 07          A=FE CY=1(borrow) AC=0
//   0019  CE 01        ACI 01          A=00 CY=1 AC=1 Z=1
//   001B  9F           SBB A           A=FF CY=1(borrow) AC=0
//   001C  32 03 40     STA 4003        [4003]=FF
//   001F  3A 00 40     LDA 4000        A=66 (flags untouched)
//   0022  B7           ORA A           flags recomputed: CY=0 AC=0 P=1
//   0023  32 04 40     STA 4004        [4004]=66
//   0026  76           HLT

#define LABSTEST_MAIN
#include "labstest.hpp"
#include <cstddef>

#include <vector>

#include "cpu.hpp"

using namespace i8080;

namespace {

const std::vector<uint8_t> kDiagProgram = {
    0x3E, 0x25,
    0xC6, 0x41,
    0x32, 0x00, 0x40,
    0x3E, 0xF0,
    0xE6, 0x3C,
    0x32, 0x01, 0x40,
    0x3E, 0xAA,
    0xEE, 0x55,
    0x32, 0x02, 0x40,
    0x3E, 0x05,
    0xD6, 0x07,
    0xCE, 0x01,
    0x9F,
    0x32, 0x03, 0x40,
    0x3A, 0x00, 0x40,
    0xB7,
    0x32, 0x04, 0x40,
    0x76,
};

struct DiagRig {
    FlatBus bus;
    Cpu cpu;

    DiagRig() {
        for (size_t i = 0; i < kDiagProgram.size(); ++i)
            bus.mem[i] = kDiagProgram[i];
        cpu.bus = &bus;
        while (!cpu.halted && cpu.cycles < 10000) cpu.step();
    }
};

}  // namespace

TEST(challenge, result_table_values) {
    DiagRig rig;
    EXPECT_EQ(rig.bus.mem[0x4000], 0x66);
    EXPECT_EQ(rig.bus.mem[0x4001], 0x30);
    EXPECT_EQ(rig.bus.mem[0x4002], 0xFF);
    EXPECT_EQ(rig.bus.mem[0x4003], 0xFF);
    EXPECT_EQ(rig.bus.mem[0x4004], 0x66);
}

TEST(challenge, accumulator_and_flags_at_halt) {
    DiagRig rig;
    EXPECT_EQ(rig.cpu.a, 0x66);
    EXPECT_FALSE(rig.cpu.s);
    EXPECT_FALSE(rig.cpu.z);
    EXPECT_TRUE(rig.cpu.p);    // 0x66 has four set bits -> even parity
    EXPECT_FALSE(rig.cpu.cy);  // ORA A cleared it last
    EXPECT_FALSE(rig.cpu.ac);
}

TEST(challenge, total_t_states) {
    DiagRig rig;
    EXPECT_EQ(rig.cpu.cycles, 156u);
}

TEST(challenge, halts_cleanly_in_budget) {
    DiagRig rig;
    EXPECT_TRUE(rig.cpu.halted);
}
