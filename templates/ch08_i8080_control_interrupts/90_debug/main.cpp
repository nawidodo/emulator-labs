#define LABSTEST_MAIN
#include "labstest.hpp"

#include "cpu.hpp"

// Debugging exercise: the skeleton carries TWO seeded bugs.
//   BUG(1): conditional CALL taken/not-taken cycle counts swapped
//   BUG(2): stack_pop reads return addresses back-to-front
// Both tests fail on the bugged skeleton and pass once fixed. Students must
// document bug / root cause / first divergence / fix / regression test in a
// bug-report.md (see DEBUGGING.md).

using namespace i8080;

namespace {

struct Rig {
    FlatBus bus;
    Cpu cpu;

    explicit Rig(const std::vector<uint8_t>& prog) {
        for (size_t i = 0; i < prog.size(); ++i) bus.mem[i] = prog[i];
        cpu.bus = &bus;
    }

    uint64_t run(uint64_t cycles) {
        while (!cpu.halted && cpu.cycles < cycles) cpu.step();
        return cpu.cycles;
    }
};

}  // namespace

TEST(bug1, conditional_call_cycles_exact) {
    // Taken call must cost exactly 17T; the skipped variant 11T.
    const std::vector<uint8_t> prog = {
        0x31, 0x00, 0x20,   // LXI SP,2000  10T
        0x3E, 0x01,         // MVI A,01      7T
        0xC4, 0x09, 0x00,   // CNZ 0009     17T taken
        0x76,               // HLT           7T
        0x3C,               // INR A         5T
        0xC9,               // RET          10T
    };
    Rig rig(prog);
    rig.run(100);
    EXPECT_EQ(rig.cpu.a, 0x02);
    EXPECT_EQ(rig.cpu.cycles, 10u + 7u + 17u + 5u + 10u + 7u);

    const std::vector<uint8_t> skipped = {
        0xAF,               // XRA A (Z=1)     4T
        0xC4, 0x09, 0x00,   // CNZ NOT taken  11T
        0x76,               // HLT             7T
    };
    Rig rig2(skipped);
    rig2.run(100);
    EXPECT_EQ(rig2.cpu.cycles, 4u + 11u + 7u);
}

TEST(bug2, returns_land_on_the_pushed_address) {
    // Nested calls: outer routine calls inner twice, then returns.
    const std::vector<uint8_t> prog = {
        0x31, 0x00, 0x20,   // 0000 LXI SP,2000
        0xCD, 0x08, 0x00,   // 0003 CALL 0008
        0x76,               // 0006 HLT
        0x00,
        0xCD, 0x0D, 0x00,   // 0008 CALL 000D (inner)
        0xC9,               // 000B RET       -> back to 0006
        0x00,
        0x3C,               // 000D INR A
        0x3C,               // 000E INR A
        0xC9,               // 000F RET       -> back to 000B
    };
    Rig rig(prog);
    rig.run(100);
    EXPECT_EQ(rig.cpu.a, 0x02);   // inner ran exactly once
    EXPECT_EQ(rig.cpu.pc, 0x0007);
    EXPECT_EQ(rig.cpu.sp, 0x2000);
}

TEST(bug2, psw_pop_survives_byte_order) {
    // POP PSW shares stack_pop: A and flags must not swap halves.
    const std::vector<uint8_t> prog = {
        0x31, 0x00, 0x20,   // LXI SP,2000
        0x3E, 0x42,         // MVI A,42
        0xF5,               // PUSH PSW
        0xAF,               // XRA A
        0xF1,               // POP PSW
        0x76,
    };
    Rig rig(prog);
    rig.run(100);
    EXPECT_EQ(rig.cpu.a, 0x42);      // if halves swap, A becomes the PSW byte
    EXPECT_FALSE(rig.cpu.z);
}
