#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>

#include "cpu.hpp"

using namespace nes6502;

namespace {

struct Rig {
    FlatRam ram;
    Cpu cpu{.bus = &ram};

    void prog(std::initializer_list<uint8_t> bytes, uint16_t base = 0x0600) {
        cpu.load_program(base, bytes.begin(), bytes.size());
    }
};

// Tiny shared handler: bump a page-zero service counter, return.
constexpr uint8_t kHandler[] = {
    0xE6, 0x30,       // INC $30
    0x40,             // RTI
};
constexpr uint16_t kHandlerAddr = 0x0700;

void install(Rig& r, uint16_t vector_lo, uint16_t target) {
    r.ram.mem[vector_lo] = uint8_t(target);
    r.ram.mem[uint16_t(vector_lo + 1)] = uint8_t(target >> 8);
    for (size_t i = 0; i < sizeof kHandler; ++i)
        r.ram.mem[kHandlerAddr + i] = kHandler[i];
}

}  // namespace

TEST(interrupts, brk_pushes_b_bit_and_vectors_through_fffe) {
    Rig r;
    install(r, 0xFFFE, kHandlerAddr);
    r.prog({0xA9, 0x42,   // 0600 LDA #$42
            0x00,         // 0602 BRK
            0xEA,         // 0603 padding byte
            0x02});       // 0604 JAM on return
    run(r.cpu, 10);

    EXPECT_EQ(r.ram.mem[0x30], 1);         // handler ran once
    EXPECT_TRUE(r.cpu.halted);
    // Stack after BRK (SP started $FD): PCH@1FD, PCL@1FC, P@1FB.
    const uint8_t stacked_p = r.ram.mem[0x01FB];
    EXPECT_TRUE((stacked_p & FB) != 0);    // THE B bit — BRK's signature
    EXPECT_TRUE((stacked_p & FU) != 0);    // unused bit always reads 1
    EXPECT_EQ(uint8_t(stacked_p & ~FB), uint8_t(FI | FU));  // I raised
}

TEST(interrupts, brk_pushed_pc_points_past_padding_byte) {
    Rig r;
    install(r, 0xFFFE, kHandlerAddr);
    r.prog({0xEA,       // 0600 NOP
            0x00,       // 0601 BRK
            0xEA,       // 0602 padding byte
            0x02});     // 0603 JAM on return
    run(r.cpu, 10);
    // Pushed PC = BRK address + 2 = $0603.
    EXPECT_EQ(r.ram.mem[0x01FC], 0x03);  // PCL
    EXPECT_EQ(r.ram.mem[0x01FD], 0x06);  // PCH
    EXPECT_EQ(r.ram.mem[0x30], 1);
}

TEST(interrupts, nmi_edge_latch_fires_exactly_once_per_edge) {
    Rig r;
    install(r, 0xFFFA, kHandlerAddr);
    // Endless NOP sled via JMP $0600: stays live for the whole window
    // (a JAM would stop the core before a rearmed NMI could be observed).
    r.prog({0xEA, 0xEA, 0xEA,
            0x4C, 0x00, 0x06});
    set_nmi_line(r.cpu, true);
    run(r.cpu, 20);            // line HELD high across many steps
    EXPECT_EQ(r.ram.mem[0x30], 1);     // one service, not twenty
}

TEST(interrupts, nmi_rearms_on_new_edge_only) {
    Rig r;
    install(r, 0xFFFA, kHandlerAddr);
    r.prog({0xEA, 0xEA, 0xEA,
            0x4C, 0x00, 0x06});   // endless NOP sled
    set_nmi_line(r.cpu, true);
    run(r.cpu, 6);
    EXPECT_EQ(r.ram.mem[0x30], 1);
    run(r.cpu, 6);
    EXPECT_EQ(r.ram.mem[0x30], 1);     // holding high stacks nothing
    set_nmi_line(r.cpu, false);        // quiet...
    set_nmi_line(r.cpu, true);         // ...new edge latches a new request
    run(r.cpu, 6);
    EXPECT_EQ(r.ram.mem[0x30], 2);
}

TEST(interrupts, reset_decrements_sp_by_three_and_fetches_fffc) {
    Rig r;
    r.ram.mem[0xFFFC] = 0x00;
    r.ram.mem[0xFFFD] = 0x08;   // reset vector -> $0800
    r.ram.mem[0x0800] = 0xE8;   // INX
    r.cpu.sp = 0x00;            // easy to check the -3
    reset(r.cpu);
    EXPECT_EQ(r.cpu.pc, 0x0800);
    EXPECT_EQ(r.cpu.sp, 0xFD);
    EXPECT_EQ(r.cpu.cycles, 7);
    EXPECT_TRUE((r.cpu.p & FI) != 0);
    EXPECT_EQ(uint8_t(r.cpu.p & (FB | FD)), 0);
}

TEST(interrupts, irq_is_masked_while_i_is_set) {
    Rig r;
    install(r, 0xFFFE, kHandlerAddr);
    r.prog({0xEA, 0xEA, 0xEA, 0x02});  // NOPs then JAM; I stays SET
    r.cpu.irq_line = true;
    run(r.cpu, 10);
    EXPECT_EQ(r.ram.mem[0x30], 0);     // never serviced
    EXPECT_TRUE(r.cpu.halted);
}

TEST(interrupts, irq_fires_after_cli_with_b_bit_clear) {
    Rig r;
    install(r, 0xFFFE, kHandlerAddr);
    r.prog({0x58,       // 0600 CLI
            0xEA,       // 0601 NOP
            0x02});     // 0602 JAM
    // A level IRQ that nobody acknowledges storms the CPU, so drive the
    // line like polite hardware: assert it, take ONE service, withdraw it.
    step(r.cpu);                       // CLI executes (I was set: no service)
    EXPECT_EQ(r.ram.mem[0x30], 0);
    r.cpu.irq_line = true;
    step(r.cpu);                       // poll -> serviced (7 cycles in)
    EXPECT_EQ(r.cpu.pc, kHandlerAddr);
    EXPECT_EQ(r.ram.mem[0x30], 0);     // handler body runs on the NEXT step
    const uint8_t stacked_p = r.ram.mem[0x01FB];
    EXPECT_FALSE(stacked_p & FB);      // hardware flows push B CLEAR
    EXPECT_TRUE(stacked_p & FU);
    r.cpu.irq_line = false;            // drop the line before the RTI
    run(r.cpu, 10);                    // handler completes, NOP, JAM
    EXPECT_EQ(r.ram.mem[0x30], 1);
    EXPECT_TRUE(r.cpu.halted);
    // RTI restored the pre-interrupt P, so I must be clear again.
    EXPECT_FALSE(r.cpu.p & FI);
}

TEST(interrupts, nmi_vectors_fffa_and_ignores_the_i_flag) {
    Rig r;
    install(r, 0xFFFA, kHandlerAddr);
    r.prog({0x78,       // 0600 SEI (I SET — NMI must not care)
            0xEA,
            0x02});     // JAM
    set_nmi_line(r.cpu, true);   // quiet -> high: latched
    run(r.cpu, 10);
    EXPECT_EQ(r.ram.mem[0x30], 1);
    const uint8_t stacked_p = r.ram.mem[0x01FB];
    EXPECT_FALSE(stacked_p & FB);
}

TEST(interrupts, interrupt_sequence_costs_seven_cycles) {
    Rig r;
    install(r, 0xFFFA, kHandlerAddr);
    r.prog({0xEA});
    set_nmi_line(r.cpu, true);
    const uint64_t t0 = r.cpu.cycles;
    step(r.cpu);                    // services NMI instead of the NOP
    EXPECT_EQ(r.cpu.cycles - t0, 7u);
}
