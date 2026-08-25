#define LABSTEST_MAIN
#include "labstest.hpp"

#include <string>

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

TEST(mov, reg_to_reg_and_memory_forms) {
    // MVI B,12 ; MOV C,B ; LXI H,3000 ; MOV M,B ; HLT
    const std::vector<uint8_t> prog = {
        0x06, 0x12,             // MVI B,12
        0x48,                   // MOV C,B
        0x21, 0x00, 0x30,       // LXI H,3000
        0x70,                   // MOV M,B
        0x76,                   // HLT
    };
    Rig rig(prog);
    rig.run(100);
    EXPECT_EQ(rig.cpu.b, 0x12);
    EXPECT_EQ(rig.cpu.c, 0x12);
    EXPECT_EQ(rig.bus.mem[0x3000], 0x12);
}

TEST(mov, timing_counts_m_access) {
    Rig mov_reg({0x47});   // MOV B,A -> 5T
    EXPECT_EQ(mov_reg.cpu.step(), 5u);

    Rig mov_mem({0x70});   // MOV M,A -> 7T (HL=0 reads bus)
    EXPECT_EQ(mov_mem.cpu.step(), 7u);
}

TEST(mvi, loads_immediate_with_timing) {
    Rig mvi_reg({0x3E, 0x5A});     // MVI A,5A -> 7T
    EXPECT_EQ(mvi_reg.cpu.step(), 7u);
    EXPECT_EQ(mvi_reg.cpu.a, 0x5A);

    Rig mvi_mem({0x21, 0x34, 0x12, 0x36, 0xAA});  // LXI H,1234 ; MVI M,AA
    mvi_mem.run(100);
    EXPECT_EQ(mvi_mem.bus.mem[0x1234], 0xAA);
}

TEST(lxi, loads_pairs_little_endian) {
    const std::vector<uint8_t> prog = {
        0x01, 0x78, 0x56,    // LXI B,5678
        0x11, 0x34, 0x12,    // LXI D,1234
        0x21, 0xCD, 0xAB,    // LXI H,ABCD
        0x76,
    };
    Rig rig(prog);
    rig.run(100);
    EXPECT_EQ(rig.cpu.bc(), 0x5678);
    EXPECT_EQ(rig.cpu.de(), 0x1234);
    EXPECT_EQ(rig.cpu.hl(), 0xABCD);
    // 3 x 10T + HLT(7)
    EXPECT_EQ(rig.cpu.cycles, 37);
}

TEST(lda_sta, direct_addressing_roundtrip) {
    const std::vector<uint8_t> prog = {
        0x3E, 0xC3,             // MVI A,C3
        0x32, 0x00, 0x40,       // STA 4000
        0x3E, 0x00,             // MVI A,00
        0x3A, 0x00, 0x40,       // LDA 4000
        0x76,
    };
    Rig rig(prog);
    rig.run(100);
    EXPECT_EQ(rig.cpu.a, 0xC3);
    EXPECT_EQ(rig.bus.mem[0x4000], 0xC3);
    // MVI(7)+STA(13)+MVI(7)+LDA(13)+HLT(7)
    EXPECT_EQ(rig.cpu.cycles, 47);
}

TEST(inr_dcr, flags_and_carry_preservation) {
    {
        // MVI C,0F ; INR C -> C=10, AC set (nibble overflow).
        const std::vector<uint8_t> prog = {0x0E, 0x0F, 0x0C, 0x76};
        Rig rig(prog);
        rig.run(100);
        EXPECT_EQ(rig.cpu.c, 0x10);
        EXPECT_TRUE(rig.cpu.ac);
        EXPECT_FALSE(rig.cpu.z);
    }
    {
        // MVI D,00 ; DCR D -> FF: sign set; AC clear because the
        // complement half-sum 0 + ~1 + 1 carries no half-borrow out.
        const std::vector<uint8_t> prog = {0x16, 0x00, 0x15, 0x76};
        Rig rig(prog);
        rig.run(100);
        EXPECT_EQ(rig.cpu.d, 0xFF);
        EXPECT_TRUE(rig.cpu.s);
        EXPECT_FALSE(rig.cpu.z);
        EXPECT_FALSE(rig.cpu.ac);
    }
    {
        // Equal low nibbles through SUI: the complement-adder quirk sets
        // AC even though nothing visibly borrows (0x20 - 0x10).
        const std::vector<uint8_t> prog2 = {0x3E, 0x20, 0xD6, 0x10, 0x76};
        Rig rig2(prog2);
        rig2.run(100);
        EXPECT_EQ(rig2.cpu.a, 0x10);
        EXPECT_FALSE(rig2.cpu.cy);
        EXPECT_TRUE(rig2.cpu.ac);
    }
    {
        // INR wrapping through zero: E=FF -> 00 sets Z and P.
        const std::vector<uint8_t> prog = {0x1E, 0xFF, 0x1C, 0x76};
        Rig rig(prog);
        rig.run(100);
        EXPECT_EQ(rig.cpu.e, 0x00);
        EXPECT_TRUE(rig.cpu.z);
        EXPECT_TRUE(rig.cpu.p);
    }
    {
        // CY must survive INR/DCR: SUB A clears it; DCR keeps it clear.
        const std::vector<uint8_t> prog = {
            0x97,               // SUB A -> CY=0, Z=1
            0x3D,               // DCR A -> A=FF, CY still 0
            0x3D,               // DCR A -> A=FE
            0x76,
        };
        Rig rig(prog);
        rig.run(100);
        EXPECT_EQ(rig.cpu.a, 0xFE);
        EXPECT_FALSE(rig.cpu.cy);
    }
}

TEST(alu_group, add_adc_sub_sbb_flags) {
    {
        // MVI A,88 ; ADI 93 -> 0x11B: A=1B, CY=1; nibble sum 8+3
        // stays inside 0-F so AC is clear.
        const std::vector<uint8_t> prog = {0x3E, 0x88, 0xC6, 0x93, 0x76};
        Rig rig(prog);
        rig.run(100);
        EXPECT_EQ(rig.cpu.a, 0x1B);
        EXPECT_TRUE(rig.cpu.cy);
        EXPECT_FALSE(rig.cpu.ac);
        // parity of 0x1B (00011011, four bits) is even -> P set
        EXPECT_TRUE(rig.cpu.p);
    }
    {
        // Carry into ADC: FF+02 -> 01 CY=1; ACI 00 -> 02 CY=0.
        const std::vector<uint8_t> prog = {
            0x3E, 0xFF,         // MVI A,FF
            0xC6, 0x02,         // ADI 02
            0xCE, 0x00,         // ACI 00
            0x76,
        };
        Rig rig(prog);
        rig.run(100);
        EXPECT_EQ(rig.cpu.a, 0x02);
        EXPECT_FALSE(rig.cpu.cy);
    }
    {
        // CPI equal clears CY and sets Z without touching A.
        const std::vector<uint8_t> prog = {
            0x3E, 0x42,         // MVI A,42
            0xFE, 0x42,         // CPI 42 -> Z=1 CY=0
            0x76,
        };
        Rig rig(prog);
        rig.run(100);
        EXPECT_EQ(rig.cpu.a, 0x42);
        EXPECT_TRUE(rig.cpu.z);
        EXPECT_FALSE(rig.cpu.cy);
    }
    {
        // SBI then SBB A collapses to zero through the borrow chain.
        const std::vector<uint8_t> prog = {
            0x3E, 0x10,         // MVI A,10
            0xDE, 0x05,         // SBI 05 -> 0A, no borrow
            0x9F,               // SBB A -> 00, Z set
            0x76,
        };
        Rig rig(prog);
        rig.run(100);
        EXPECT_EQ(rig.cpu.a, 0x00);
        EXPECT_TRUE(rig.cpu.z);
        EXPECT_FALSE(rig.cpu.cy);
    }
}

TEST(alu_group, logical_ops_clear_or_quirk_ac) {
    {
        // XRA A: canonical zero/flag-clear idiom.
        const std::vector<uint8_t> prog = {0xAF, 0x76};
        Rig rig(prog);
        rig.run(100);
        EXPECT_EQ(rig.cpu.a, 0x00);
        EXPECT_TRUE(rig.cpu.z);
        EXPECT_TRUE(rig.cpu.p);
        EXPECT_FALSE(rig.cpu.cy);
        EXPECT_FALSE(rig.cpu.ac);
    }
    {
        // ANI with bit 3 set in either operand sets AC (8080 quirk).
        const std::vector<uint8_t> prog = {
            0x3E, 0x08,         // MVI A,08
            0xE6, 0xF0,         // ANI F0 -> result 00 but AC set by quirk
            0x76,
        };
        Rig rig(prog);
        rig.run(100);
        EXPECT_EQ(rig.cpu.a, 0x00);
        EXPECT_TRUE(rig.cpu.z);
        EXPECT_TRUE(rig.cpu.ac);   // (08|F0)&08 != 0
        EXPECT_FALSE(rig.cpu.cy);
    }
    {
        // ORA merges bits; S+P reflect the result.
        const std::vector<uint8_t> prog = {
            0x06, 0xF0,         // MVI B,F0
            0x3E, 0x0F,         // MVI A,0F
            0xB0,               // ORA B -> FF
            0x76,
        };
        Rig rig(prog);
        rig.run(100);
        EXPECT_EQ(rig.cpu.a, 0xFF);
        EXPECT_TRUE(rig.cpu.s);
        EXPECT_TRUE(rig.cpu.p);
        EXPECT_FALSE(rig.cpu.ac);
    }
}

TEST(step_result, every_instruction_reports_cycles) {
    // Timing spot checks (curriculum §56: step() returns its cost).
    Rig nop({0x00});
    EXPECT_EQ(nop.cpu.step(), 4u);

    Rig inr_b({0x04});           // INR B -> 5T
    EXPECT_EQ(inr_b.cpu.step(), 5u);

    Rig hlt({0x76});
    EXPECT_EQ(hlt.cpu.step(), 7u);
    EXPECT_TRUE(hlt.cpu.halted);
    // HLT makes further steps no-ops returning zero cost.
    EXPECT_EQ(hlt.cpu.step(), 0u);
}

TEST(disassemble, decodes_subset_mnemonics) {
    Rig rig({0x3E, 0x12, 0xC6, 0x34, 0x32, 0x00, 0x40, 0x76});
    EXPECT_EQ(rig.cpu.disassemble(0x0000), "MVI A,#12");
    EXPECT_EQ(rig.cpu.disassemble(0x0002), "ADI #34");
    EXPECT_EQ(rig.cpu.disassemble(0x0004), "STA 4000");
    EXPECT_EQ(rig.cpu.disassemble(0x0007), "HLT");
}
