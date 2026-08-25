#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <vector>

#include "cpu.hpp"

using namespace nes6502;

namespace {

struct Rig {
    RecordingBus bus;
    Cpu cpu{.bus = &bus};

    void prog(std::initializer_list<uint8_t> bytes, uint16_t base = 0x0600) {
        cpu.load_program(base, bytes.begin(), bytes.size());
        bus.log.clear();  // ignore the program-load writes
    }

    // All write accesses logged during one instruction.
    std::vector<Access> writes() const {
        std::vector<Access> w;
        for (const auto& a : bus.log)
            if (a.write) w.push_back(a);
        return w;
    }
    std::vector<Access> reads() const {
        std::vector<Access> r;
        for (const auto& a : bus.log)
            if (!a.write) r.push_back(a);
        return r;
    }
};

}  // namespace

// ---------------------------------------------------------------------------
// Dummy accesses on official ops.
// ---------------------------------------------------------------------------

TEST(dummy, rmw_writes_old_value_back_first) {
    Rig r;
    r.bus.mem[0x40] = 0xAA;
    r.prog({0xE6, 0x40});          // INC $40
    step(r.cpu);
    EXPECT_EQ(r.bus.mem[0x40], 0xAB);
    // The bus must see: read $40, write $40=AA (dummy), write $40=AB.
    const auto w = r.writes();
    EXPECT_EQ(w.size(), 2u);
    EXPECT_EQ(w[0].addr, 0x40);
    EXPECT_EQ(w[0].value, 0xAA);   // the ORIGINAL value hits the bus
    EXPECT_EQ(w[1].value, 0xAB);
}

TEST(dummy, rmw_cycle_totals_include_the_dummy_write) {
    Rig r;
    r.bus.mem[0x0040] = 1;
    r.prog({0xE6, 0x40});          // INC $40   -> 5 cycles
    step(r.cpu);
    EXPECT_EQ(r.cpu.cycles, 5u);

    Rig r2;
    r2.bus.mem[0x2040] = 1;
    r2.prog({0xEE, 0x40, 0x20});   // INC $2040 -> 6 cycles
    step(r2.cpu);
    EXPECT_EQ(r2.cpu.cycles, 6u);
}

TEST(dummy, indexed_store_performs_speculative_read_at_wrong_address) {
    Rig r;
    r.cpu.x = 0x02;
    r.cpu.a = 0x5A;
    r.prog({0x9D, 0xFE, 0x20});    // STA $20FE,X -> real target $2100
    step(r.cpu);
    EXPECT_EQ(r.bus.mem[0x2100], 0x5A);
    // No page cross, but indexed stores ALWAYS do the speculative read at
    // the un-fixed-up address {base_high, sum_low} = $2000 here.
    bool saw_speculative = false;
    for (const auto& a : r.bus.log)
        if (!a.write && a.addr == 0x2000) saw_speculative = true;
    EXPECT_TRUE(saw_speculative);
    EXPECT_EQ(r.cpu.cycles, 5u);
}

TEST(dummy, indexed_read_pays_dummy_only_on_page_cross) {
    Rig r;
    r.bus.mem[0x2007] = 0x11;
    r.cpu.x = 0x02;
    r.prog({0xBD, 0x05, 0x20});    // LDA $2005,X -> $2007, no cross: 4 cyc
    step(r.cpu);
    EXPECT_EQ(r.cpu.a, 0x11);
    EXPECT_EQ(r.cpu.cycles, 4u);

    Rig r2;
    r2.bus.mem[0x2101] = 0x22;
    r2.cpu.x = 0x02;
    r2.prog({0xBD, 0xFF, 0x20});   // LDA $20FF,X -> $2101: crossed
    step(r2.cpu);
    EXPECT_EQ(r2.cpu.a, 0x22);
    EXPECT_EQ(r2.cpu.cycles, 5u);  // +1 penalty...
    bool saw_fixup = false;        // ...billed as a REAL read at the
    for (const auto& a : r2.bus.log)   // speculative address $2001
        if (!a.write && a.addr == 0x2001) saw_fixup = true;
    EXPECT_TRUE(saw_fixup);
}

TEST(dummy, internal_penalty_bills_without_touching_the_bus) {
    Rig r;
    r.bus.mem[0x0010] = 0x33;
    r.cpu.x = 0x00;
    r.prog({0xB5, 0x10});          // LDA $10,X
    const size_t accesses_before = r.bus.log.size();
    step(r.cpu);
    EXPECT_EQ(r.cpu.cycles, 4u);   // opcode+operand+read+1 internal
    EXPECT_EQ(r.bus.log.size(), accesses_before + 3);  // internal invisible
}

TEST(dummy, unofficial_absx_nop_still_reads_like_a_load) {
    Rig r;
    r.cpu.x = 0x02;
    r.prog({0x1C, 0xFF, 0x20});    // NOP $20FF,X (unofficial): crossed
    step(r.cpu);
    EXPECT_EQ(r.cpu.cycles, 5u);   // same timing as LDA abs,X

    Rig r2;
    r2.prog({0xEA});
    step(r2.cpu);
    EXPECT_EQ(r2.cpu.cycles, 2u);
}

// ---------------------------------------------------------------------------
// Unofficial opcode semantics.
// ---------------------------------------------------------------------------

TEST(dummy, lax_loads_a_and_x) {
    Rig r;
    r.bus.mem[0x40] = 0x9C;
    r.prog({0xA7, 0x40});          // LAX $40
    step(r.cpu);
    EXPECT_EQ(r.cpu.a, 0x9C);
    EXPECT_EQ(r.cpu.x, 0x9C);
    EXPECT_TRUE(r.cpu.p & FN);     // N from the loaded value
}

TEST(dummy, sax_stores_a_and_x_without_flags) {
    Rig r;
    r.cpu.a = 0xF0;
    r.cpu.x = 0x3C;
    r.prog({0x87, 0x40});          // SAX $40
    step(r.cpu);
    EXPECT_EQ(r.bus.mem[0x40], 0x30);  // A & X
    EXPECT_EQ(uint8_t(r.cpu.p & FN), 0);
}

TEST(dummy, dcp_decs_then_compares) {
    Rig r;
    r.bus.mem[0x42] = 0x01;
    r.cpu.a = 0x01;
    r.prog({0xC7, 0x42});          // DCP $42: mem->0, CMP 1 vs 0 -> C set
    step(r.cpu);
    EXPECT_EQ(r.bus.mem[0x42], 0x00);
    EXPECT_TRUE(r.cpu.p & FC);
    EXPECT_FALSE(r.cpu.p & FZ);
}

TEST(dummy, isb_incs_then_sbcs) {
    Rig r;
    r.bus.mem[0x43] = 0x0F;
    r.cpu.a = 0x10;
    r.cpu.p |= FC;                 // no borrow
    r.prog({0xE7, 0x43});          // ISB $43: mem->0x10, SBC -> A=0
    step(r.cpu);
    EXPECT_EQ(r.bus.mem[0x43], 0x10);
    EXPECT_EQ(r.cpu.a, 0x00);
    EXPECT_TRUE(r.cpu.p & FZ);
}

TEST(dummy, slo_asls_then_ora) {
    Rig r;
    r.bus.mem[0x44] = 0x40;
    r.cpu.a = 0x01;
    r.prog({0x07, 0x44});          // SLO $44: mem 40->80 (bit7 was 0, so no
    step(r.cpu);                   // carry), ORA -> A=81, N set
    EXPECT_EQ(r.bus.mem[0x44], 0x80);
    EXPECT_EQ(r.cpu.a, 0x81);
    EXPECT_FALSE(r.cpu.p & FC);
    EXPECT_TRUE(r.cpu.p & FN);
}

TEST(dummy, rla_rols_then_and) {
    Rig r;
    r.bus.mem[0x45] = 0x80;
    r.cpu.a = 0xFF;
    r.prog({0x27, 0x45});          // RLA $45: mem 80->00 (C out), AND -> 0
    step(r.cpu);
    EXPECT_EQ(r.bus.mem[0x45], 0x00);
    EXPECT_EQ(r.cpu.a, 0x00);
    EXPECT_TRUE(r.cpu.p & FC);
    EXPECT_TRUE(r.cpu.p & FZ);
}

TEST(dummy, unofficial_rmw_combos_keep_the_double_write) {
    Rig r;
    r.bus.mem[0x50] = 0x81;
    r.prog({0xC7, 0x50});          // DCP $50
    step(r.cpu);
    const auto w = r.writes();
    EXPECT_EQ(w.size(), 2u);       // dummy write of 0x81, then final 0x80
    EXPECT_EQ(w[0].value, 0x81);
    EXPECT_EQ(w[1].value, 0x80);
    EXPECT_EQ(r.cpu.cycles, 5u);
}
