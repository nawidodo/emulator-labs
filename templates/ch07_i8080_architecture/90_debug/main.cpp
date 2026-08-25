#define LABSTEST_MAIN
#include "labstest.hpp"

#include <vector>

#include "cpu.hpp"

using namespace i8080;

namespace {

// Load a program at 0x0000 and run up to `cycles` T-states.
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

// --- Sanity pin: baseline ALU behavior must hold in every variant. ------

TEST(sanity, add_wraps_and_sets_carries) {
    // MVI A,FF ; ADI 01 ; HLT  -> A=00, CY=1, AC=1, Z=1
    Rig rig({0x3E, 0xFF, 0xC6, 0x01, 0x76});
    rig.run(100);
    EXPECT_EQ(rig.cpu.a, 0x00);
    EXPECT_TRUE(rig.cpu.cy);
    EXPECT_TRUE(rig.cpu.ac);
    EXPECT_TRUE(rig.cpu.z);
}

// --- BUG(1): DCR must preserve CY. --------------------------------------

TEST(dcr, preserves_carry_on_underflow) {
    // MVI A,80 ; ADI 80 (CY=1) ; MVI B,00 ; DCR B ; HLT
    // -> B=FF and CY stays SET (this core has no STC; use ALU carry).
    Rig rig({0x3E, 0x80, 0xC6, 0x80, 0x06, 0x00, 0x05, 0x76});
    rig.run(100);
    EXPECT_EQ(rig.cpu.b, 0xFF);
    EXPECT_TRUE(rig.cpu.cy);   // stub clears/mangles this
}

TEST(dcr, preserves_cleared_carry) {
    // MVI C,05 ; DCR C ; HLT -> CY stays CLEAR.
    Rig rig({0x0E, 0x05, 0x0D, 0x76});
    rig.run(100);
    EXPECT_EQ(rig.cpu.c, 0x04);
    EXPECT_FALSE(rig.cpu.cy);
}

// --- BUG(2): CMP computes flags without writing A. ----------------------

TEST(cmp, leaves_accumulator_untouched) {
    // MVI A,10 ; CPI 20 ; HLT -> A still 0x10; flags: CY=1 (borrow), S=0.
    Rig rig({0x3E, 0x10, 0xFE, 0x20, 0x76});
    rig.run(100);
    EXPECT_EQ(rig.cpu.a, 0x10);   // stub clobbers A with the compare result
    EXPECT_TRUE(rig.cpu.cy);
    EXPECT_FALSE(rig.cpu.z);
}

// --- BUG(3): LDA fetches its address low byte first. --------------------

TEST(lda, address_operand_is_low_byte_first) {
    // Program at 0: LDA 0x2000 ; HLT. Memory at 0x2000 holds 7F.
    Rig rig({0x3A, 0x00, 0x20, 0x76});
    rig.bus.mem[0x2000] = 0x7F;
    rig.run(100);
    EXPECT_EQ(rig.cpu.a, 0x7F);   // stub reads from the swapped address
}

TEST(sta, stores_at_low_byte_first_address) {
    // STA 0x3000 with A=5B, then inspect memory.
    Rig rig({0x3E, 0x5B, 0x32, 0x00, 0x30, 0x76});
    rig.run(100);
    EXPECT_EQ(rig.bus.mem[0x3000], 0x5B);
}
