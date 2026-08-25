#define LABSTEST_MAIN
#include <span>

#include "labstest.hpp"
#include "../03_ld_alu/bus.hpp"
#include "../03_ld_alu/cpu.hpp"
#include "ldh.hpp"

namespace {

void run_ldh(gb::Cpu& cpu, gb::FlatBus& bus, std::span<const uint8_t> program,
             int max_steps = 200000) {
    bus.mem.fill(0);
    bus.load(program);
    cpu = gb::Cpu{};
    cpu.bus = &bus;
    cpu.extra_exec = gbtest::ldh_exec;
    for (int i = 0; i < max_steps; ++i) {
        if (cpu.halted || cpu.trap) break;
        cpu.step();
    }
}

}  // namespace

TEST(ldh, e0_f0_roundtrip) {
    const uint8_t prog[] = {
        0x3E, 0x77,        // ld a,$77
        0xE0, 0x30,        // ldh ($30),a   -> FF30 = 77
        0x21, 0x00, 0xC0,  // ld hl,$C000
        0xF0, 0x30,        // ldh a,($30)   -> a = FF30
        0x77,              // ld (hl),a
        0x76,
    };
    gb::Cpu cpu;
    gb::FlatBus bus;
    run_ldh(cpu, bus, prog);
    EXPECT_EQ(bus.mem[0xFF30], 0x77);
    EXPECT_EQ(cpu.a, 0x77);
}

TEST(ldh, e2_f2_use_c_register) {
    const uint8_t prog[] = {
        0x0E, 0x40,        // ld c,$40
        0x3E, 0xEE,        // ld a,$EE
        0xE2,              // ldh (c),a     -> FF40 = EE
        0xAF,              // xor a         -> a = 0
        0xF2,              // ldh a,(c)     -> a = EE
        0x76,
    };
    gb::Cpu cpu;
    gb::FlatBus bus;
    run_ldh(cpu, bus, prog);
    EXPECT_EQ(bus.mem[0xFF40], 0xEE);
    EXPECT_EQ(cpu.a, 0xEE);
}

TEST(ldh, e8_f8_sp_relative_flags) {
    gb::Cpu cpu;
    gb::FlatBus bus;
    cpu.bus = &bus;

    bus.mem[0x0100] = 0x0F;  // shared operand byte
    cpu.pc = 0x0100;
    cpu.sp = 0x000F;
    int cycles = 0;
    EXPECT_TRUE(gbtest::ldh_exec(cpu, 0xE8, cycles));  // add SP,+15
    EXPECT_EQ(cycles, 16);
    EXPECT_EQ(cpu.sp, 0x001E);
    EXPECT_TRUE(cpu.flag_h());   // $F+$F overflows the nibble
    EXPECT_FALSE(cpu.flag_c());  // $0F+$0F stays inside the byte
    EXPECT_FALSE(cpu.flag_z());

    bus.mem[0x0100] = 0xF0;      // -16
    cpu.pc = 0x0100;
    cpu.sp = 0x0010;
    cycles = 0;
    EXPECT_TRUE(gbtest::ldh_exec(cpu, 0xF8, cycles));  // ld HL,SP-16
    EXPECT_EQ(cycles, 12);
    EXPECT_EQ(cpu.hl(), 0x0000);
    EXPECT_FALSE(cpu.flag_h());  // $0+$0 no nibble carry
    EXPECT_TRUE(cpu.flag_c());   // $10+$F0 overflows the byte
}
