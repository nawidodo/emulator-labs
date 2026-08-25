#define LABSTEST_MAIN
#include "labstest.hpp"
#include "interp_debug.hpp"

using namespace psx::r3000a;

namespace {
constexpr uint32_t rtype(uint32_t rs_, uint32_t rt_, uint32_t rd_, uint32_t f) {
    return (rs_ << 21) | (rt_ << 16) | (rd_ << 11) | f;
}
constexpr uint32_t itype(uint32_t op, uint32_t rs_, uint32_t rt_, uint16_t imm) {
    return (op << 26) | (rs_ << 21) | (rt_ << 16) | imm;
}

struct Machine {
    Bus bus;
    CpuState cpu;
};

void run(Machine& m, int n) {
    for (int i = 0; i < n && !m.cpu.halted; ++i) cpu_step(m.cpu, m.bus);
}
}  // namespace

// Each group isolates ONE seeded defect. All groups must pass after the fix.

TEST(bug1_advance_polarity, taken_branch_must_transfer_control) {
    Window w{0x100u, 0x104u, false};
    const Window n = advance(w, true, 0x200u);
    EXPECT_EQ(n.current_pc, 0x104u);   // delay slot runs first
    EXPECT_EQ(n.next_pc, 0x200u);      // then the target
    const Window s = advance(w, false, 0x200u);
    EXPECT_EQ(s.next_pc, 0x108u);      // untaken: purely sequential
}

TEST(bug1_advance_polarity, program_reaches_branch_target) {
    Machine m;
    // beq $zero,$zero,+1 -> skip the poison addiu; syscall
    const uint32_t prog[] = {
        itype(0x04, 0, 0, 0x0002),   // beq always taken over the poison
        0,                            // nop (delay slot)
        itype(0x09, 0, 8, 99),        // poison: skipped by the branch
        rtype(0, 0, 0, 0x0C),         // syscall halt
    };
    m.cpu.load_program(m.bus,
                       reinterpret_cast<const uint8_t*>(prog), sizeof prog);
    run(m, 10);
    EXPECT_TRUE(m.cpu.halted);
    EXPECT_EQ(m.cpu.regs.gpr[8], 0u);  // poison never ran
}

TEST(bug2_branch_base, displacement_counts_from_delay_slot) {
    EXPECT_EQ(branch_target(0x100u, -1), 0x100u);
    EXPECT_EQ(branch_target(0x100u, 2), 0x10Cu);
}

TEST(bug3_link_address, jal_links_past_the_slot) {
    EXPECT_EQ(link_address(0x100u), 0x108u);
}

TEST(bug3_link_address, return_does_not_rerun_slot) {
    Machine m;
    // jal func ; nop(slot) ; sw $t0,0x400 ; syscall ... func: $t0+=5 ; jr $ra ; $t0+=1 (slot)
    const uint32_t base = kProgramBase;
    const uint32_t prog[] = {
        (0x03 << 26) | (((base + 0x20u) >> 2) & 0x03FFFFFFu),  // jal func
        0,
        itype(0x2B, 0, 8, 0x400),
        rtype(0, 0, 0, 0x0C),
        0, 0, 0, 0,
        itype(0x09, 8, 8, 5),   // func body
        rtype(31, 0, 0, 0x08),  // jr $ra
        itype(0x09, 8, 8, 1),   // jr's own delay slot
    };
    m.cpu.load_program(m.bus,
                       reinterpret_cast<const uint8_t*>(prog), sizeof prog);
    run(m, 30);
    EXPECT_TRUE(m.cpu.halted);
    // Slot of jr ran exactly once (during the call), and control resumed at
    // the sw AFTER the call's slot — not on it again.
    EXPECT_EQ(m.cpu.regs.gpr[8], 6u);
    EXPECT_EQ(do_lw(m.bus, 0x400u), 6u);
}

TEST(bug4_jump_target, callee_entry_runs_in_full) {
    Machine m;
    // jal func ; nop ; syscall ... func: sw $t9 marker word at 0x404
    const uint32_t base = kProgramBase;
    const uint32_t prog[] = {
        (0x03 << 26) | (((base + 0x18u) >> 2) & 0x03FFFFFFu),  // jal func
        0,                                                     // slot
        rtype(0, 0, 0, 0x0C),                                  // syscall
        0,
        0, 0,
        itype(0x09, 0, 25, 0x777),                             // func: $t9=0x777
        rtype(31, 0, 0, 0x08),                                 // jr $ra
    };
    // NOTE: func entry is base+0x18; its FIRST instruction must execute.
    m.cpu.load_program(m.bus,
                       reinterpret_cast<const uint8_t*>(prog), sizeof prog);
    run(m, 30);
    EXPECT_TRUE(m.cpu.halted);
    EXPECT_EQ(m.cpu.regs.gpr[25], 0x777u);  // callee's first instruction ran
}

TEST(bug5_delay_flag, window_reports_delay_slots) {
    Window w{0x100u, 0x104u, false};
    EXPECT_EQ(advance(w, true, 0x200u).in_delay_slot, true);
    EXPECT_EQ(in_delay_slot_after(true), true);
    EXPECT_EQ(in_delay_slot_after(false), false);
}
