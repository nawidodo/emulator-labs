#define LABSTEST_MAIN
#include "labstest.hpp"

#include "cpu.hpp"

using namespace i8080;

namespace {

struct Rig {
    FlatBus bus;
    Cpu cpu;

    explicit Rig(const std::vector<uint8_t>& prog, uint16_t origin = 0) {
        for (size_t i = 0; i < prog.size(); ++i) bus.mem[origin + i] = prog[i];
        cpu.bus = &bus;
    }

    uint64_t run(uint64_t cycles) {
        while (!cpu.halted && cpu.cycles < cycles) cpu.step();
        return cpu.cycles;
    }
};

}  // namespace

TEST(jmp, unconditional_and_timing) {
    const std::vector<uint8_t> prog = {
        0x3E, 0x01,         // 0000 MVI A,01
        0xC3, 0x06, 0x00,   // 0002 JMP 0006 (skips the MVI below)
        0x3E, 0xFF,         // 0005 (skipped) MVI A,FF -- byte shared
        0x76,               // 0006 HLT
    };
    Rig rig(prog);
    rig.run(100);
    EXPECT_EQ(rig.cpu.a, 0x01);
}

TEST(jcc, taken_vs_not_taken_cycles) {
    const std::vector<uint8_t> prog = {
        0xAF,               // 0000 XRA A       (Z=1)      4T
        0xC2, 0x08, 0x00,   // 0001 JNZ 0008    NOT taken  7T
        0xCA, 0x08, 0x00,   // 0004 JZ 0008     taken     10T
        0x00,               // 0007 NOP
        0x76,               // 0008 HLT                    7T
    };
    Rig rig(prog);
    rig.run(100);
    EXPECT_EQ(rig.cpu.pc, 0x09);
    EXPECT_EQ(rig.cpu.cycles, 4 + 7 + 10 + 7);
}

TEST(call_ret, roundtrip_with_stack_balance) {
    const std::vector<uint8_t> prog = {
        0x31, 0x00, 0x20,   // 0000 LXI SP,2000   10T
        0x3E, 0x00,         // 0003 MVI A,00       7T
        0xCD, 0x09, 0x00,   // 0005 CALL 0009     17T
        0x76,               // 0008 HLT            7T
        0x3C,               // 0009 INR A          5T
        0xC9,               // 000A RET           10T
    };
    Rig rig(prog);
    rig.run(100);
    EXPECT_EQ(rig.cpu.a, 0x01);
    EXPECT_EQ(rig.cpu.sp, 0x2000);        // balanced
    EXPECT_EQ(rig.cpu.pc, 0x0009);        // halted after the return
    EXPECT_EQ(rig.cpu.cycles, 10 + 7 + 17 + 5 + 10 + 7);
}

TEST(ccc, not_taken_call_costs_eleven) {
    const std::vector<uint8_t> prog = {
        0xAF,               // 0000 XRA A (Z=1)     4T
        0xC4, 0x09, 0x00,   // 0001 CNZ NOT taken  11T
        0x76,               // 0004 HLT             7T
        0x00, 0x00,
    };
    Rig rig(prog);
    rig.run(100);
    EXPECT_EQ(rig.cpu.pc, 0x05);
    EXPECT_EQ(rig.cpu.cycles, 4 + 11 + 7);
}

TEST(ret_cc, skipped_return_is_five_cycles) {
    {
        const std::vector<uint8_t> prog = {
            0x31, 0x00, 0x20,   // LXI SP,2000  10T
            0xAF,               // XRA A (Z=1)   4T
            0xC0,               // RNZ NOT taken 5T
            0x76,               // HLT           7T
        };
        Rig rig(prog);
        rig.run(100);
        EXPECT_EQ(rig.cpu.sp, 0x2000);    // stack untouched by skip
        EXPECT_EQ(rig.cpu.cycles, 10 + 4 + 5 + 7);
    }
    {
        const std::vector<uint8_t> prog = {
            0x31, 0x00, 0x20,   // 0000 LXI SP,2000 10T
            0xAF,               // 0003 XRA A (Z=1)  4T
            0xCD, 0x09, 0x00,   // 0004 CALL 0009   17T
            0x76,               // 0007 HLT          7T
            0x00,               // 0008 padding
            0xC8,               // 0009 RZ taken    11T
        };
        Rig rig(prog);
        rig.run(100);
        EXPECT_EQ(rig.cpu.pc, 0x0008);
        EXPECT_EQ(rig.cpu.sp, 0x2000);
        EXPECT_EQ(rig.cpu.cycles, 10 + 4 + 17 + 11 + 7);
    }
}

TEST(push_pop, pairs_and_psw) {
    {
        const std::vector<uint8_t> prog = {
            0x31, 0x00, 0x20,   // LXI SP,2000
            0x01, 0x34, 0x12,   // LXI B,1234
            0xC5,               // PUSH B      11T
            0x01, 0x00, 0x00,   // LXI B,0000
            0xC1,               // POP B       10T
            0x76,
        };
        Rig rig(prog);
        rig.run(100);
        EXPECT_EQ(rig.cpu.bc(), 0x1234);      // survived the round trip
        EXPECT_EQ(rig.cpu.sp, 0x2000);
        EXPECT_EQ(rig.bus.mem[0x1FFF], 0x12); // high byte pushed first
    }
    {
        // PUSH PSW / POP PSW must preserve CY through intervening flag clears.
        const std::vector<uint8_t> prog = {
            0x31, 0x00, 0x20,   // LXI SP,2000
            0x3E, 0x88,         // MVI A,88
            0xC6, 0x93,         // ADI 93 -> A=1B CY=1
            0xF5,               // PUSH PSW   11T
            0xAF,               // XRA A (clears everything)
            0xF1,               // POP PSW    10T
            0x76,
        };
        Rig rig(prog);
        rig.run(100);
        EXPECT_EQ(rig.cpu.a, 0x1B);
        EXPECT_TRUE(rig.cpu.cy);              // restored from the stack
        EXPECT_TRUE(rig.cpu.p);               // parity bit came back too
        EXPECT_FALSE(rig.cpu.z);
    }
}

TEST(rst, vectors_through_low_memory) {
    const std::vector<uint8_t> handler = {0x3C, 0xC9};   // INR A ; RET
    const std::vector<uint8_t> prog = {
        0x31, 0x00, 0x20,   // LXI SP,2000  10T
        0xCF,               // RST 1 -> vector 0008  11T
        0x76,               // HLT            7T
    };
    Rig rig(prog);
    for (size_t i = 0; i < handler.size(); ++i)
        rig.bus.mem[0x0008 + i] = handler[i];
    rig.run(100);
    EXPECT_EQ(rig.cpu.a, 0x01);
    EXPECT_EQ(rig.cpu.pc, 0x0005);
    EXPECT_EQ(rig.cpu.sp, 0x2000);
    EXPECT_EQ(rig.cpu.cycles, 10 + 11 + 5 + 10 + 7);
}

TEST(pchl_sphl_xchg, register_transfers) {
    {
        const std::vector<uint8_t> prog = {
            0x21, 0x05, 0x00,   // 0000 LXI H,0005 10T
            0xE9,               // 0003 PCHL        5T
            0x00,               // 0004 (skipped)
            0x76,               // 0005 HLT         7T
        };
        Rig rig(prog);
        rig.run(100);
        EXPECT_EQ(rig.cpu.pc, 0x0006);
        EXPECT_EQ(rig.cpu.cycles, 10 + 5 + 7);
    }
    {
        const std::vector<uint8_t> prog = {
            0x21, 0x00, 0x40,   // LXI H,4000
            0xF9,               // SPHL 5T
            0xEB,               // XCHG 4T
            0x76,
        };
        Rig rig(prog);
        rig.run(100);
        EXPECT_EQ(rig.cpu.sp, 0x4000);
        EXPECT_EQ(rig.cpu.hl(), 0x0000);   // DE was 0 before the swap
        EXPECT_EQ(rig.cpu.de(), 0x4000);
    }
}

TEST(interrupts, gated_by_di_accepted_after_ei) {
    const std::vector<uint8_t> handler = {0x3C, 0xFB, 0xC9};  // INR A ; EI ; RET
    const std::vector<uint8_t> prog = {
        0x31, 0x00, 0x20,   // 0000 LXI SP,2000
        0xFB,               // 0003 EI
        0xF3,               // 0004 DI
        0xFB,               // 0005 EI
        0x00,               // 0006 NOP  <- interrupted here
        0x76,               // 0007 HLT
    };
    Rig rig(prog);
    for (size_t i = 0; i < handler.size(); ++i)
        rig.bus.mem[0x0010 + i] = handler[i];

    // While IFF is clear (after DI), the interrupt is IGNORED entirely.
    rig.cpu.step();                    // LXI SP      -> pc=3
    rig.cpu.step();                    // EI          -> pc=4
    rig.cpu.step();                    // DI          -> pc=5
    EXPECT_FALSE(rig.cpu.interrupt(0xD7));   // RST 2 refused
    EXPECT_EQ(rig.cpu.pc, 0x0005);     // PC untouched

    // After EI the same interrupt is accepted and vectored to 0x10.
    rig.cpu.step();                    // EI          -> pc=6
    EXPECT_TRUE(rig.cpu.interrupt(0xD7));
    EXPECT_EQ(rig.cpu.pc, 0x0010);     // RST 2 vector
    EXPECT_FALSE(rig.cpu.iff);         // acknowledge clears IFF
    rig.run(500);                      // handler: INR A; EI; RET
    EXPECT_EQ(rig.cpu.a, 0x01);
    EXPECT_EQ(rig.cpu.sp, 0x2000);
    EXPECT_EQ(rig.cpu.pc, 0x0008);     // resumed, ran the NOP, halted
}

TEST(interrupts, wakes_halt_and_pushes_resume_point) {
    const std::vector<uint8_t> handler = {0xFB, 0xC9};  // EI ; RET
    Rig rig({0x76});                    // 0000 HLT
    for (size_t i = 0; i < handler.size(); ++i)
        rig.bus.mem[0x0020 + i] = handler[i];   // RST 4 vector = 0020

    rig.cpu.step();                     // HLT
    EXPECT_TRUE(rig.cpu.halted);

    EXPECT_EQ(rig.cpu.step(), 0u);      // stepping while halted costs nothing
    EXPECT_TRUE(rig.cpu.interrupt(0xE7));   // RST 4 accepted despite HALT
    EXPECT_FALSE(rig.cpu.halted);       // woke up

    rig.bus.mem[0x0001] = 0x76;         // park a HLT at the resume point
    rig.run(100);
    EXPECT_EQ(rig.cpu.pc, 0x0002);      // RET resumed exactly where pushed
}
