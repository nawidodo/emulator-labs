#define LABSTEST_MAIN
#include <span>

#include "labstest.hpp"
#include "../01_daa_rotates/bus.hpp"
#include "../01_daa_rotates/core.hpp"
#include "stack_ops.hpp"

namespace {

void run(gb::Cpu& cpu, gb::FlatBus& bus, std::span<const uint8_t> program,
         int max_steps = 200000) {
    bus.mem.fill(0);
    bus.load(program);
    cpu = gb::Cpu{};
    cpu.bus = &bus;
    gb::install_stack_hook(cpu);
    for (int i = 0; i < max_steps; ++i) {
        if (cpu.halted || cpu.trap) break;
        cpu.step();
    }
}

}  // namespace

TEST(stack, push_pop_roundtrip) {
    gb::Cpu cpu;
    gb::FlatBus bus;
    cpu.bus = &bus;
    cpu.sp = 0xFFFE;
    gb::push16(cpu, 0xBEEF);
    EXPECT_EQ(cpu.sp, 0xFFFC);
    EXPECT_EQ(bus.mem[0xFFFD], 0xBE);  // high byte at higher address
    EXPECT_EQ(bus.mem[0xFFFC], 0xEF);
    EXPECT_EQ(gb::pop16(cpu), 0xBEEF);
    EXPECT_EQ(cpu.sp, 0xFFFE);
}

TEST(stack, pop_af_masks_low_nibble) {
    gb::Cpu cpu;
    gb::FlatBus bus;
    cpu.bus = &bus;
    cpu.sp = 0xC100;
    bus.mem[0xC100] = 0xCD;  // low byte (would-be F)
    bus.mem[0xC101] = 0xAB;  // high byte (A)
    gb::pop_af(cpu);
    EXPECT_EQ(cpu.a, 0xAB);
    EXPECT_EQ(cpu.f, 0xC0);  // low nibble dropped
}

TEST(prog, call_ret_nesting) {
    const uint8_t prog[] = {
        // @0100
        0x3E, 0x07,        // ld a,7
        0xCD, 0x20, 0x01,  // call $0120
        0x21, 0x00, 0xC0,  // ld hl,$C000
        0x86,              // add a,(hl)
        0x76,              // @109: halt
        // pad $010A..$011F so the subroutine sits exactly at $0120
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        // @0120: subroutine doubles A and returns
        0x87,              // add a,a -> 14
        0xC9,              // ret
    };
    gb::Cpu cpu;
    gb::FlatBus bus;
    bus.mem.fill(0);
    bus.load(prog);
    bus.mem[0xC000] = 1;  // operand read by add a,(hl)
    cpu = gb::Cpu{};
    cpu.bus = &bus;
    gb::install_stack_hook(cpu);
    for (int i = 0; i < 200000 && !cpu.halted && !cpu.trap; ++i) cpu.step();
    EXPECT_EQ(cpu.a, 15);  // (7*2)+1
}

TEST(timing, call_cc_not_taken_base_price_only) {
    gb::Cpu cpu;
    gb::FlatBus bus;
    cpu.bus = &bus;
    // Reset F=$B0 has Z set, so "call nz" is NOT taken.
    int cycles = 12;
    cpu.pc = 0x0100;
    bus.mem[0x0100] = 0x34;
    bus.mem[0x0101] = 0x12;
    EXPECT_TRUE(gb::stack_exec(cpu, 0xC4 /*call nz,nn*/, cycles));
    EXPECT_EQ(cycles, 12);
    EXPECT_EQ(cpu.pc, 0x0102);      // fell through
    EXPECT_EQ(cpu.sp, 0xFFFE);      // nothing pushed
}

TEST(timing, call_cc_taken_costs_24) {
    gb::Cpu cpu;
    gb::FlatBus bus;
    cpu.bus = &bus;
    cpu.f = 0;                      // Z clear -> nz taken
    int cycles = 12;
    cpu.pc = 0x0100;
    cpu.sp = 0xFFFE;
    bus.mem[0x0100] = 0x20;
    bus.mem[0x0101] = 0x01;
    EXPECT_TRUE(gb::stack_exec(cpu, 0xC4 /*call nz,nn*/, cycles));
    EXPECT_EQ(cycles, 24);
    EXPECT_EQ(cpu.pc, 0x0120);
    EXPECT_EQ(cpu.sp, 0xFFFC);
    // return address on the stack is $0103
    EXPECT_EQ(bus.mem[0xFFFC], 0x02);
    EXPECT_EQ(bus.mem[0xFFFD], 0x01);
}

TEST(timing, ret_cc_not_taken_vs_unconditional) {
    gb::Cpu cpu;
    gb::FlatBus bus;
    cpu.bus = &bus;
    cpu.f = 0xB0;                   // Z set -> ret nz NOT taken
    int cycles = 8;
    EXPECT_TRUE(gb::stack_exec(cpu, 0xC0 /*ret nz*/, cycles));  // handled
    EXPECT_EQ(cycles, 8);                                       // not taken

    cycles = 16;
    cpu.sp = 0xC000;
    bus.mem[0xC000] = 0x50;
    bus.mem[0xC001] = 0x01;
    EXPECT_TRUE(gb::stack_exec(cpu, 0xC9 /*ret*/, cycles));
    EXPECT_EQ(cycles, 16);
    EXPECT_EQ(cpu.pc, 0x0150);
}

TEST(rst, vector_and_return_address) {
    gb::Cpu cpu;
    gb::FlatBus bus;
    cpu.bus = &bus;
    cpu.pc = 0x0123;
    cpu.sp = 0xFFFE;
    int cycles = 0;
    EXPECT_TRUE(gb::stack_exec(cpu, 0xDF /*rst $18*/, cycles));
    EXPECT_EQ(cpu.pc, 0x0018);
    EXPECT_EQ(bus.mem[0xFFFC], 0x23);
    EXPECT_EQ(bus.mem[0xFFFD], 0x01);
    EXPECT_EQ(cycles, 16);
}
