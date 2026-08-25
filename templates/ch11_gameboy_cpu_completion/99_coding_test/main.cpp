#define LABSTEST_MAIN
#include "labstest.hpp"
#include <cstddef>

#include "../01_daa_rotates/bus.hpp"
#include "../01_daa_rotates/core.hpp"
#include "ldh.hpp"

namespace {

struct Fixture {
    gb::FlatBus bus;
    gb::Cpu cpu;
    Fixture() {
        cpu.bus = &bus;
        gb::install_ldh_hook(cpu);
    }
};

}  // namespace

TEST(ldh, e0_full_semantics_and_cycles) {
    Fixture f;
    f.bus.mem[0x0100] = 0x80;  // operand n = $80 for the fetch below
    f.cpu.pc = 0x0100;
    f.cpu.a = 0x5A;
    int cycles = 0;
    EXPECT_TRUE(gb::ldh_exec(f.cpu, 0xE0, cycles));
    EXPECT_EQ(f.cpu.pc, 0x0101);
    EXPECT_EQ(f.bus.mem[0xFF80], 0x5A);
    EXPECT_EQ(cycles, 12);
}

TEST(ldh, f0_reads_hram_into_a) {
    Fixture f;
    f.bus.mem[0x0100] = 0x23;
    f.cpu.pc = 0x0100;
    f.bus.mem[0xFF23] = 0x77;
    int cycles = 0;
    EXPECT_TRUE(gb::ldh_exec(f.cpu, 0xF0, cycles));
    EXPECT_EQ(f.cpu.a, 0x77);
    EXPECT_EQ(cycles, 12);
}

TEST(ldh, e2_f2_use_register_c_window) {
    Fixture f;
    f.cpu.c = 0x42;
    f.cpu.a = 0x99;
    int cycles = 0;
    EXPECT_TRUE(gb::ldh_exec(f.cpu, 0xE2, cycles));
    EXPECT_EQ(f.bus.mem[0xFF42], 0x99);
    EXPECT_EQ(cycles, 12);

    f.bus.mem[0xFF42] = 0x11;
    cycles = 0;
    EXPECT_TRUE(gb::ldh_exec(f.cpu, 0xF2, cycles));
    EXPECT_EQ(f.cpu.a, 0x11);
    EXPECT_EQ(cycles, 12);
}

TEST(ldh, opcode_08_stores_sp_little_endian_20_cycles) {
    Fixture f;
    f.bus.mem[0x0100] = 0x00;
    f.bus.mem[0x0101] = 0xD0;  // nn = $D000
    f.cpu.pc = 0x0100;
    f.cpu.sp = 0xE000;
    int cycles = 0;
    EXPECT_TRUE(gb::ldh_exec(f.cpu, 0x08, cycles));
    EXPECT_EQ(f.bus.mem[0xD000], 0x00);
    EXPECT_EQ(f.bus.mem[0xD001], 0xE0);
    EXPECT_EQ(cycles, 20);
}

TEST(ldh, family_touches_no_flags_and_rejects_other_opcodes) {
    Fixture f;
    f.cpu.f = 0xB0;  // Z N H C all set: every family op must preserve this
    f.bus.mem[0x0100] = 0x00;
    f.cpu.pc = 0x0100;
    f.cpu.a = 0xAA;
    int cycles = 0;
    (void)gb::ldh_exec(f.cpu, 0xE0, cycles);
    (void)gb::ldh_exec(f.cpu, 0xF0, cycles);
    (void)gb::ldh_exec(f.cpu, 0xE2, cycles);
    (void)gb::ldh_exec(f.cpu, 0xF2, cycles);
    (void)gb::ldh_exec(f.cpu, 0x08, cycles);
    EXPECT_EQ(f.cpu.f, 0xB0);

    int untouched = -1;
    EXPECT_FALSE(gb::ldh_exec(f.cpu, 0xEA, untouched));
    EXPECT_FALSE(gb::ldh_exec(f.cpu, 0x76, untouched));
}

TEST(ldh, hook_chain_end_to_end_through_cpu_step) {
    Fixture f;
    const uint8_t prog[] = {
        0x3E, 0x21,              // ld a,$21
        0xE0, 0x7F,              // ldh ($7F),a
        0x76,                    // halt
    };
    for (size_t i = 0; i < sizeof(prog); ++i)
        f.bus.mem[static_cast<uint16_t>(0x0100 + i)] = prog[i];
    // Bounded loop: the skeleton's step() stub never raises trap or
    // advances cyc, so an unbounded while would hang the RED run.
    for (int i = 0; i < 1000 && !f.cpu.halted && !f.cpu.trap; ++i)
        f.cpu.step();
    EXPECT_TRUE(f.cpu.halted);
    EXPECT_FALSE(f.cpu.trap);
    EXPECT_EQ(f.bus.mem[0xFF7F], 0x21);
}
