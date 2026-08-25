#define LABSTEST_MAIN
#include <span>

#include "labstest.hpp"
#include "bus.hpp"
#include "core.hpp"
#include "daa_ops.hpp"

namespace {

void run(gb::Cpu& cpu, gb::FlatBus& bus, std::span<const uint8_t> program,
         int max_steps = 200000) {
    bus.mem.fill(0);
    bus.load(program);
    cpu = gb::Cpu{};
    cpu.bus = &bus;
    gb::install_daa_hook(cpu);
    for (int i = 0; i < max_steps; ++i) {
        if (cpu.halted || cpu.trap) break;
        cpu.step();
    }
}

}  // namespace

TEST(daa, after_bcd_add_with_half_carry) {
    // 0x45 + 0x38 = 0x7D with H=1 -> BCD 83
    gb::Cpu cpu;
    gb::FlatBus bus;
    cpu.bus = &bus;
    cpu.a = 0x7D;
    cpu.set_n(false);
    cpu.set_h(true);
    cpu.set_c(false);
    gb::daa(cpu);
    EXPECT_EQ(cpu.a, 0x83);
    EXPECT_FALSE(cpu.flag_z());
    EXPECT_FALSE(cpu.flag_h());
}

TEST(daa, after_add_overflow_sets_carry) {
    // BCD 99 + 01: raw 0x9A -> DAA gives 0x00 with C=1
    gb::Cpu cpu;
    gb::FlatBus bus;
    cpu.bus = &bus;
    cpu.a = 0x9A;
    cpu.set_n(false);
    cpu.set_h(false);
    cpu.set_c(false);
    gb::daa(cpu);
    EXPECT_EQ(cpu.a, 0x00);
    EXPECT_TRUE(cpu.flag_z());
    EXPECT_TRUE(cpu.flag_c());
}

TEST(daa, after_sub_with_borrow_nibble) {
    // BCD 40 - 25: raw 0x1B with N=1 H=1 -> 15
    gb::Cpu cpu;
    gb::FlatBus bus;
    cpu.bus = &bus;
    cpu.a = 0x1B;
    cpu.set_n(true);
    cpu.set_h(true);
    cpu.set_c(false);
    gb::daa(cpu);
    EXPECT_EQ(cpu.a, 0x15);
    EXPECT_FALSE(cpu.flag_c());
}

TEST(daa, after_sub_full_borrow_keeps_carry) {
    // BCD 32 - 99: raw 0x93 with N=1 C=1 -> 33
    gb::Cpu cpu;
    gb::FlatBus bus;
    cpu.bus = &bus;
    cpu.a = 0x93;
    cpu.set_n(true);
    cpu.set_h(false);
    cpu.set_c(true);
    gb::daa(cpu);
    EXPECT_EQ(cpu.a, 0x33);
    EXPECT_TRUE(cpu.flag_c());  // carry preserved on subtract path
}

TEST(rot, rl_shifts_carry_into_bit0) {
    gb::Cpu cpu;
    gb::FlatBus bus;
    cpu.bus = &bus;
    cpu.f = 0;
    cpu.set_c(true);
    const uint8_t r = gb::rotate_left(cpu, 0x80);  // RL of $80 with C=1
    EXPECT_EQ(r, 0x01);         // old C lands in bit 0
    EXPECT_TRUE(cpu.flag_c());  // new C = old bit 7
}

TEST(rot, rlc_shifts_in_zero_ignores_old_carry) {
    gb::Cpu cpu;
    gb::FlatBus bus;
    cpu.bus = &bus;
    cpu.f = 0;
    const uint8_t r = gb::rotate_left_carry(cpu, 0x80);
    EXPECT_EQ(r, 0x01);  // RLC rotates bit 7 into bit 0 (old carry ignored)
    EXPECT_TRUE(cpu.flag_c());
}

TEST(rot, sra_keeps_sign_bit) {
    gb::Cpu cpu;
    gb::FlatBus bus;
    cpu.bus = &bus;
    cpu.f = 0;
    const uint8_t r = gb::shift_arith_right(cpu, 0x81);
    EXPECT_EQ(r, 0xC0);
    EXPECT_TRUE(cpu.flag_c());
}

TEST(rot, swap_flag_contract) {
    gb::Cpu cpu;
    gb::FlatBus bus;
    cpu.bus = &bus;
    cpu.f = 0;
    cpu.set_c(true);
    const uint8_t r = gb::swap_nibbles(cpu, 0x2E);
    EXPECT_EQ(r, 0xE2);
    EXPECT_FALSE(cpu.flag_c());
    gb::finish_cb(cpu, r);
    EXPECT_FALSE(cpu.flag_z());
}

TEST(cb, bit_reads_without_touching_carry) {
    gb::Cpu cpu;
    gb::FlatBus bus;
    cpu.bus = &bus;
    cpu.b = 0x02;
    cpu.f = 0;
    cpu.set_c(true);
    int cycles = 0;
    EXPECT_TRUE(gb::cb_exec(cpu, 0x40 /*bit 0,b*/, cycles));
    EXPECT_TRUE(cpu.flag_z());   // bit 0 of $02 is clear
    EXPECT_TRUE(cpu.flag_h());
    EXPECT_TRUE(cpu.flag_c());   // untouched by BIT
    EXPECT_EQ(cycles, 8);
}

TEST(cb, res_set_leave_flags_alone) {
    gb::Cpu cpu;
    gb::FlatBus bus;
    cpu.bus = &bus;
    cpu.b = 0x00;
    cpu.f = 0xFF;
    int cycles = 0;
    gb::cb_exec(cpu, 0xC0 /*set 0,b*/, cycles);
    EXPECT_EQ(cpu.f, 0xFF);  // SET never touches F
    EXPECT_EQ(cpu.b, 0x01);
    gb::cb_exec(cpu, 0x87 /*res 0,a*/, cycles);  // A boots as $01
    EXPECT_EQ(cpu.a, 0x00);
    EXPECT_EQ(cpu.f, 0xFF);
}

TEST(prog, rlca_forces_zero_flag) {
    const uint8_t prog[] = {
        0x3E, 0x40,  // ld a,$40
        0xB7,        // or a   -> Z=0 (a != 0)
        0x07,        // rlca   -> a=$80, C=1
        0xAF,        // xor a  -> Z=1
        0x07,        // rlca of 0 -> Z forced back to 0!
        0x76,
    };
    gb::Cpu cpu;
    gb::FlatBus bus;
    run(cpu, bus, prog);
    EXPECT_EQ(cpu.a, 0x00);
    EXPECT_FALSE(cpu.flag_z());  // RLCA clears Z unconditionally
}
