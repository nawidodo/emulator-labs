#define LABSTEST_MAIN
#include <cstring>
#include <span>

#include "labstest.hpp"
#include "bus.hpp"
#include "cpu.hpp"

namespace {

void run(gb::Cpu& cpu, gb::FlatBus& bus, std::span<const uint8_t> program,
         int max_steps = 200000) {
    bus.mem.fill(0);
    bus.load(program);
    cpu = gb::Cpu{};
    cpu.bus = &bus;
    // Step-count bound (not cycle bound): an unimplemented stub advances
    // neither PC nor cyc, and skeletons must still terminate.
    for (int i = 0; i < max_steps; ++i) {
        if (cpu.halted || cpu.trap) break;
        cpu.step();
    }
}

}  // namespace
TEST(alu, add_flags) {
    gb::FlatBus bus;
    gb::Cpu cpu;
    cpu.bus = &bus;

    cpu.a = 0x3C;
    cpu.f = 0;
    cpu.alu8(0, cpu.a, 0x0F, false, true);  // ADD A,0x0F -> half-carry only
    EXPECT_EQ(cpu.a, 0x4B);
    EXPECT_TRUE(cpu.flag_h());
    EXPECT_FALSE(cpu.flag_c());
    EXPECT_FALSE(cpu.flag_z());
    EXPECT_FALSE(cpu.flag_n());

    cpu.a = 0xFF;
    cpu.alu8(0, cpu.a, 1, false, true);  // ADD A,1 -> carry + zero
    EXPECT_EQ(cpu.a, 0x00);
    EXPECT_TRUE(cpu.flag_z());
    EXPECT_TRUE(cpu.flag_c());
}

TEST(alu, adc_folds_carry_in) {
    gb::FlatBus bus;
    gb::Cpu cpu;
    cpu.bus = &bus;
    cpu.set_c(true);
    cpu.a = 0x10;
    const uint8_t r = cpu.alu8(1, cpu.a, 0x01, true, true);  // ADC A,1
    EXPECT_EQ(r, 0x12);
    EXPECT_FALSE(cpu.flag_h());
}

TEST(alu, sub_and_cp) {
    gb::FlatBus bus;
    gb::Cpu cpu;
    cpu.bus = &bus;
    cpu.a = 0x3E;
    cpu.alu8(2, cpu.a, 0x3F, false, true);  // SUB -> borrow everywhere
    EXPECT_EQ(cpu.a, 0xFF);
    EXPECT_TRUE(cpu.flag_n());
    EXPECT_TRUE(cpu.flag_h());
    EXPECT_TRUE(cpu.flag_c());

    gb::Cpu c2;
    c2.bus = &bus;
    c2.a = 0x42;
    c2.alu8(7, c2.a, 0x42, false, false);  // CP equal -> Z, N=1
    EXPECT_EQ(c2.a, 0x42);
    EXPECT_TRUE(c2.flag_z());
    EXPECT_TRUE(c2.flag_n());
}

TEST(alu, logic_flag_rules) {
    gb::FlatBus bus;
    gb::Cpu cpu;
    cpu.bus = &bus;
    cpu.set_c(true);
    cpu.set_h(true);
    cpu.set_n(true);
    cpu.alu8(4, 0x0F, 0xF0, false, true);  // AND: H pinned to 1
    EXPECT_EQ(cpu.a, 0x00);
    EXPECT_TRUE(cpu.flag_z());
    EXPECT_TRUE(cpu.flag_h());
    EXPECT_FALSE(cpu.flag_n());
    EXPECT_FALSE(cpu.flag_c());

    cpu.alu8(5, 0xAA, 0xAA, false, true);  // XOR same value -> zero
    EXPECT_EQ(cpu.a, 0x00);
    EXPECT_TRUE(cpu.flag_z());
    EXPECT_FALSE(cpu.flag_h());

    cpu.set_c(true);
    cpu.alu8(6, 0x12, 0x34, false, true);  // OR clears C/H/N
    EXPECT_EQ(cpu.a, 0x36);
    EXPECT_FALSE(cpu.flag_c());
}

TEST(alu, inc_dec_keep_carry) {
    gb::FlatBus bus;
    gb::Cpu cpu;
    cpu.bus = &bus;
    cpu.set_c(true);

    uint8_t v = 0x0F;
    cpu.inc8(v);
    EXPECT_EQ(v, 0x10);
    EXPECT_TRUE(cpu.flag_h());   // nibble overflowed
    EXPECT_FALSE(cpu.flag_z());
    EXPECT_TRUE(cpu.flag_c());   // preserved

    v = 0x01;
    cpu.dec8(v);
    EXPECT_EQ(v, 0x00);
    EXPECT_TRUE(cpu.flag_z());
    EXPECT_TRUE(cpu.flag_n());
    EXPECT_TRUE(cpu.flag_c());   // still preserved

    v = 0x00;
    cpu.dec8(v);
    EXPECT_EQ(v, 0xFF);
    EXPECT_TRUE(cpu.flag_h());   // borrow from bit 4
}

TEST(ld, immediate_loads_and_pairs) {
    const uint8_t prog[] = {
        0x01, 0x34, 0x12,  // ld bc,$1234
        0x21, 0x00, 0xC0,  // ld hl,$C000
        0x3E, 0x7F,        // ld a,$7F
        0x77,              // ld (hl),a
        0x76,              // halt
    };
    gb::Cpu cpu;
    gb::FlatBus bus;
    run(cpu, bus, prog);
    EXPECT_EQ(bus.mem[0xC000], 0x7F);
    EXPECT_TRUE(cpu.halted);
}
TEST(ld, ldi_loop_16bit_ops) {
    const uint8_t prog[] = {
        0x21, 0x10, 0xC0,       // @100: ld hl,$C010
        0x0E, 0x03,             // ld c,3
        0x3E, 0x05,             // ld a,5
                                // loop @103:
        0x22,                   // ldi (hl),a
        0x0D,                   // dec c
        0x20, 0xFC,             // jr nz,loop (offset -4)
        0x39,                   // add hl,sp (runs once after loop exits)
        0x76,                   // halt
    };
    gb::Cpu cpu;
    gb::FlatBus bus;
    run(cpu, bus, prog);
    EXPECT_EQ(cpu.c, 0);
    EXPECT_EQ(bus.mem[0xC010], 5);
    EXPECT_EQ(bus.mem[0xC011], 5);
    EXPECT_EQ(bus.mem[0xC012], 5);
    EXPECT_EQ(cpu.hl(), 0xC011);  // $C013 + $FFFE wraps to $C011
}

TEST(step, cycle_accounting_and_trap) {
    const uint8_t prog[] = {
        0x00,              // nop          4
        0x3E, 0x01,        // ld a,1       8
        0x18, 0x03,        // jr +3 taken 12 (skips three pad bytes)
        0xFB, 0xFB, 0xFB,  // skipped padding
        0x76,              // halt         4
    };
    gb::Cpu cpu;
    gb::FlatBus bus;
    run(cpu, bus, prog);
    EXPECT_TRUE(cpu.halted);
    EXPECT_EQ(cpu.cyc, 4 + 8 + 12 + 4);

    // Reset F=$B0 has Z set; LD does not touch flags, so JR NZ is NOT
    // taken: 8 (ld) + 8 (jr, not taken) + 4 (halt).
    const uint8_t not_taken[] = {
        0x3E, 0x01,  // ld a,1        8
        0x20, 0x05,  // jr nz,+5      8 (condition false)
        0x76,        // halt          4
    };
    gb::Cpu c2;
    run(c2, bus, not_taken);
    EXPECT_TRUE(c2.halted);
    EXPECT_EQ(c2.cyc, 20);
    EXPECT_EQ(c2.pc, 0x0105);  // halt at $0104, PC advanced past it
}
