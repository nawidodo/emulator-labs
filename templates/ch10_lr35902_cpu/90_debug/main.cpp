#define LABSTEST_MAIN
#include <span>

#include "labstest.hpp"
#include "cpu_dbg.hpp"

namespace {

void run(gbdbg::Cpu& cpu, gb::FlatBus& bus, std::span<const uint8_t> program,
         int max_steps = 200000) {
    bus.mem.fill(0);
    bus.load(program);
    cpu = gbdbg::Cpu{};
    cpu.bus = &bus;
    for (int i = 0; i < max_steps; ++i) {
        if (cpu.halted || cpu.trap) break;
        cpu.step();
    }
}

}  // namespace

TEST(alu, adc_includes_carry_in) {
    gb::FlatBus bus;
    gbdbg::Cpu cpu;
    cpu.bus = &bus;
    cpu.set_c(true);
    cpu.a = 0x10;
    const uint8_t r = cpu.arith8(1, cpu.a, 0x01, true);  // ADC A,1
    EXPECT_EQ(r, 0x12);
    EXPECT_FALSE(cpu.flag_h());
    EXPECT_FALSE(cpu.flag_c());
}

TEST(alu, sbc_borrows_carry_in) {
    gb::FlatBus bus;
    gbdbg::Cpu cpu;
    cpu.bus = &bus;
    cpu.set_c(true);
    const uint8_t r = cpu.arith8(3, 0x10, 0x00, true);  // SBC A,0 with C=1
    EXPECT_EQ(r, 0x0F);
}

TEST(alu, cp_leaves_accumulator_alone) {
    gb::FlatBus bus;
    gbdbg::Cpu cpu;
    cpu.bus = &bus;
    cpu.a = 0x42;
    const uint8_t r = cpu.logic8(7, cpu.a, 0x99);  // CP 0x99: smaller
    EXPECT_EQ(cpu.a, 0x42);
    EXPECT_EQ(r, 0xA9);  // 42-99 wraps to A9
    EXPECT_TRUE(cpu.flag_n());
    EXPECT_TRUE(cpu.flag_c());
}

TEST(alu, logic_ops_untouched_flags_contract) {
    gb::FlatBus bus;
    gbdbg::Cpu cpu;
    cpu.bus = &bus;
    cpu.a = 0xFF;
    cpu.logic8(4, cpu.a, 0x0F);  // AND
    EXPECT_EQ(cpu.a, 0xFF);      // logic ops never write via lhs either
}

TEST(timing, jr_not_taken_costs_base_cycles) {
    const uint8_t prog[] = {
        0x3E, 0x01,  // ld a,1        8   (F keeps reset Z -> NZ false)
        0x20, 0x05,  // jr nz,+5      8   not taken
        0x76,        // halt          4
    };
    gbdbg::Cpu cpu;
    gb::FlatBus bus;
    run(cpu, bus, prog);
    EXPECT_TRUE(cpu.halted);
    EXPECT_EQ(cpu.cyc, 20);
}
TEST(timing, jr_taken_pays_extra_mcycle) {
    const uint8_t prog[] = {
        0x3E, 0x01,  // @100: ld a,1      8
        0xB7,        // @102: or a        4   -> Z=0 (a != 0)
        0x20, 0x01,  // @103: jr nz,+1   12   taken
        0x00,        // @105: nop (skipped)
        0x76,        // @106: halt        4
    };
    gbdbg::Cpu cpu;
    gb::FlatBus bus;
    run(cpu, bus, prog);
    EXPECT_TRUE(cpu.halted);
    EXPECT_EQ(cpu.cyc, 28);
}
