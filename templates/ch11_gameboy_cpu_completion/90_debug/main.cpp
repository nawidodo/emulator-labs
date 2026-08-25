#define LABSTEST_MAIN
#include "labstest.hpp"
#include "debug_cpu.hpp"

namespace {

using gbdbg::DbgCpu;

}  // namespace

// ---- debug_daa: BCD adjust with a half-carry ----------------------------

TEST(debug_daa, add_with_half_carry_adjusts_low_nibble) {
    // 0x45 + 0x38 = 0x7D raw, H set -> DAA must produce BCD 83.
    DbgCpu cpu;
    cpu.a = 0x7D;
    cpu.set_n(false);
    cpu.set_h(true);
    cpu.set_c(false);
    gbdbg::daa_add_path(cpu);
    EXPECT_EQ(cpu.a, 0x83);
    EXPECT_FALSE(cpu.flag_z());
}

TEST(debug_daa, double_adjust_still_works_after_fix) {
    // BCD 99 + 01: raw 0x9A needs BOTH the +0x60 and +0x06 adjustments.
    DbgCpu cpu;
    cpu.a = 0x9A;
    cpu.set_n(false);
    cpu.set_h(true);
    cpu.set_c(false);
    gbdbg::daa_add_path(cpu);
    EXPECT_EQ(cpu.a, 0x00);
    EXPECT_TRUE(cpu.flag_c());
    EXPECT_TRUE(cpu.flag_z());
}

TEST(debug_daa, subtraction_path_untouched_by_the_fix) {
    // BCD 40 - 25: raw 0x1B with N=1 H=1 -> 15. Guards against "fixing"
    // bug 1 by breaking the N=1 half-carry path.
    DbgCpu cpu;
    cpu.a = 0x1B;
    cpu.set_n(true);
    cpu.set_h(true);
    gbdbg::daa_add_path(cpu);
    EXPECT_EQ(cpu.a, 0x15);
}

// ---- debug_cb: CB-page flag contract ------------------------------------

TEST(debug_cb, srl_to_zero_raises_z_flag) {
    // SRL of 0x01 yields 0x00: CB form must set Z FROM THE RESULT.
    DbgCpu cpu;
    const uint8_t res = gbdbg::cb_srl(cpu, 0x01);
    EXPECT_EQ(res, 0x00);
    EXPECT_TRUE(cpu.flag_z());
    EXPECT_FALSE(cpu.flag_n());
    EXPECT_FALSE(cpu.flag_h());
    EXPECT_TRUE(cpu.flag_c());  // shifted-out bit 0 was 1
}

TEST(debug_cb, nonzero_result_keeps_z_clear_and_swaps_keep_c_low) {
    DbgCpu cpu;
    (void)gbdbg::cb_srl(cpu, 0xFE);  // result 0x7F != 0
    EXPECT_FALSE(cpu.flag_z());
    EXPECT_FALSE(cpu.flag_c());
}

// ---- debug_halt: sane HALT resume ---------------------------------------

TEST(debug_halt, halt_sleeps_and_resume_pc_is_next_instruction) {
    // Program fragment at $0100: 76 (halt) 3C (inc a). After the HALT
    // opcode is fetched, PC already points at the `inc a`; waking with
    // IME clear must resume THERE -- not two bytes later.
    DbgCpu cpu;
    cpu.mem[0x0100] = 0x76;  // halt
    cpu.mem[0x0101] = 0x3C;  // inc a   <- wake lands here
    cpu.mem[0x0102] = 0x00;  // nop     <- bug(3) swallows this too
    cpu.fetch8();            // fetch HALT; pc now 0x0101
    gbdbg::dbg_halt(cpu);
    EXPECT_TRUE(cpu.halted);
    EXPECT_EQ(gbdbg::halt_resume_pc(cpu), 0x0101);
}

TEST(debug_halt, resume_executes_the_next_instruction_not_a_skip) {
    // End-to-end flavor of the same defect: park a marker store right
    // after HALT and check it survives a wake.
    DbgCpu cpu;
    cpu.mem[0x0100] = 0x76;              // halt
    cpu.mem[0x0101] = 0xEA;              // ld ($C000),a
    cpu.mem[0x0102] = 0x00;
    cpu.mem[0x0103] = 0xC0;
    cpu.a = 0x42;
    cpu.fetch8();                        // consume HALT
    gbdbg::dbg_halt(cpu);
    EXPECT_TRUE(cpu.halted);  // woke by the driver below
    cpu.pc = gbdbg::halt_resume_pc(cpu);
    // execute `ld ($C000),a` by hand: fetch opcode + operand bytes
    const uint8_t op = cpu.fetch8();
    const uint16_t lo = cpu.fetch8();
    const uint16_t hi = cpu.fetch8();
    EXPECT_EQ(op, 0xEA);
    if (op == 0xEA) cpu.write(static_cast<uint16_t>(lo | hi << 8), cpu.a);
    EXPECT_EQ(cpu.read(0xC000), 0x42);
}

// ---- debug_jr: taken-branch cycle delta ---------------------------------

TEST(debug_jr, taken_branch_costs_twelve_cycles) {
    DbgCpu cpu;
    cpu.mem[0x0100] = 0x05;  // displacement byte (already fetched next)
    cpu.mem[0x0101] = 0x00;
    const int cycles = gbdbg::jr_cc(cpu, true);
    EXPECT_EQ(cycles, 12);
    EXPECT_EQ(cpu.pc, 0x0106);  // pc after fetch (0x0101) + displacement
}

TEST(debug_jr, not_taken_fall_through_costs_eight) {
    DbgCpu cpu;
    cpu.mem[0x0100] = 0x02;
    const int cycles = gbdbg::jr_cc(cpu, false);
    EXPECT_EQ(cycles, 8);
    EXPECT_EQ(cpu.pc, 0x0101);  // consumed the displacement, no jump
}
