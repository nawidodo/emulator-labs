#define LABSTEST_MAIN
#include "labstest.hpp"

#include <span>

#include "cpu.hpp"

namespace {

ch02::Cpu loaded(std::initializer_list<uint8_t> program) {
    ch02::Cpu c;
    c.load(std::span<const uint8_t>(program.begin(), program.size()));
    return c;
}

}  // namespace

TEST(flow, store_writes_memory) {
    // 0: LOAD r1,#0xAB   3: STORE r1,0x80   6: HALT
    auto c = loaded({0x10, 0x01, 0xAB, 0x20, 0x01, 0x80, 0x00});
    const uint32_t spent = c.run(100);
    EXPECT_EQ(c.ram[0x80], 0xAB);
    EXPECT_EQ(spent, 4u + 6u + 4u);  // STORE costs 6, memory is slower
}

TEST(flow, loadm_reads_memory) {
    ch02::Cpu c;
    c.load(std::span<const uint8_t>({0x30, 0x03, 0x42, 0x00}));
    c.ram[0x42] = 0x7E;              // preload after load() reset the RAM
    (void)c.run(100);
    EXPECT_EQ(c.r[3], 0x7E);
}

TEST(flow, store_loadm_roundtrip_through_0xff) {
    // LOAD r2,#0x5A ; STORE r2,0xFF ; LOAD r2,#0 ; LOADM r2,0xFF ; HALT
    auto c = loaded({0x10, 0x02, 0x5A, 0x20, 0x02, 0xFF,
                     0x10, 0x02, 0x00, 0x30, 0x02, 0xFF, 0x00});
    (void)c.run(100);
    EXPECT_EQ(c.ram[0xFF], 0x5A);
    EXPECT_EQ(c.r[2], 0x5A);
}

TEST(flow, jmp_lands_exactly_on_target) {
    // 0: JMP 0x05 ; 2..4: filler ; 5: LOAD r0,#9 ; 8: HALT
    auto c = loaded({0x60, 0x05, 0x99, 0x99, 0x99,
                     0x10, 0x00, 0x09, 0x00});
    const uint32_t spent = c.run(100);
    EXPECT_EQ(c.r[0], 9);            // reachable only via an exact jump
    EXPECT_TRUE(c.halted);
    EXPECT_EQ(spent, 3u + 4u + 4u);  // JMP costs 3
}

TEST(flow, jmp_does_not_advance_pc_a_second_time) {
    // A double-advanced PC lands at target+1 and executes garbage.
    // 0: JMP 0x03 ; 2: filler ; 3: SUB r1,r1 (sets Z) ; 6: HALT
    auto c = loaded({0x60, 0x03, 0x99,
                     0x50, 0x01, 0x01,
                     0x00});
    (void)c.run(100);
    EXPECT_TRUE(c.halted);
    EXPECT_EQ(c.r[1], 0);            // executed only if pc hit 0x03 exactly
    EXPECT_EQ(c.pc, 7);              // stopped cleanly at end, not past it
}

TEST(flow, jz_taken_jumps_and_costs_3_cycles) {
    // 0: LOAD r0,#0x40 ; 3: SUB r0,r0 -> Z=1 ; 6: JZ 0x09 ;
    // 8: poison byte (must be skipped) ; 9: HALT
    auto c = loaded({0x10, 0x00, 0x40, 0x50, 0x00, 0x00,
                     0x70, 0x09, 0xFF, 0x00});
    const uint32_t spent = c.run(100);
    EXPECT_EQ(c.r[0], 0);
    EXPECT_TRUE(c.halted);
    EXPECT_EQ(spent, 4u + 4u + 3u + 4u);
    EXPECT_EQ(c.pc, 10);
}

TEST(flow, jz_not_taken_falls_through_and_costs_2_cycles) {
    // 0: LOAD r0,#0x40 (no flag change) ; 3: JZ 0x08 (Z=0 -> fall through) ;
    // 5: LOAD r1,#7 ; 8: HALT
    auto c = loaded({0x10, 0x00, 0x40, 0x70, 0x08,
                     0x10, 0x01, 0x07, 0x00});
    const uint32_t spent = c.run(100);
    EXPECT_EQ(c.r[1], 7);            // fall-through executed
    EXPECT_EQ(spent, 4u + 2u + 4u + 4u);
}

TEST(flow, run_stops_at_cycle_budget_without_halting) {
    // JMP 0x00 forever: 3 cycles per instruction, never halts. Budget
    // exhaustion leaves the machine running (callers decide what that means).
    auto c = loaded({0x60, 0x00});
    const uint32_t spent = c.run(30);
    EXPECT_EQ(spent, 30u);
    EXPECT_FALSE(c.halted);
}

TEST(flow, countdown_loop_terminates) {
    // 0: LOAD r0,#7     3: LOAD r1,#1
    // 6: SUB r0,r1   <- loop
    // 9: JZ done(0x0F)
    // B: JMP loop(0x06)
    // D: padding        F: HALT <- done
    auto c = loaded({0x10, 0x00, 0x07, 0x10, 0x01, 0x01,
                     0x50, 0x00, 0x01,
                     0x70, 0x0F,
                     0x60, 0x06,
                     0x00, 0x00,
                     0x00});
    const uint32_t spent = c.run(500);
    EXPECT_TRUE(c.halted);
    EXPECT_EQ(c.r[0], 0);
    // 2 LOADs + 6 iterations x (SUB+JZ-not-taken+JMP=9) + final SUB+JZ-taken
    // (7) + HALT (4)
    EXPECT_EQ(spent, 73u);
}

TEST(flow, flags_survive_memory_and_flow_ops) {
    // ADD raises carry+zero; STORE/LOADM/JMP must not disturb either.
    // 0: LOAD r0,#255  3: LOAD r1,#1   6: ADD r0,r1 (0, Z=1, C=1)
    // 9: STORE r0,0x80  C: LOADM r2,0x80  F: JMP 0x11  ; 11: HALT
    auto c = loaded({0x10, 0x00, 0xFF, 0x10, 0x01, 0x01, 0x40, 0x00, 0x01,
                     0x20, 0x00, 0x80, 0x30, 0x02, 0x80, 0x60, 0x11,
                     0x00, 0x00});
    (void)c.run(100);
    EXPECT_TRUE(c.carry);
    EXPECT_TRUE(c.zero);
    EXPECT_EQ(c.r[2], 0);
}
