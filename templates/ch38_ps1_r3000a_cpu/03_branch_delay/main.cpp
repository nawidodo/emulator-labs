#define LABSTEST_MAIN
#include "labstest.hpp"
#include "interp.hpp"
#include "trace.hpp"

using namespace psx::r3000a;

namespace {
// Assemble a handful of instructions inline for readability.
constexpr uint32_t rtype(uint32_t rs_, uint32_t rt_, uint32_t rd_, uint32_t f) {
    return (rs_ << 21) | (rt_ << 16) | (rd_ << 11) | f;
}
constexpr uint32_t itype(uint32_t op, uint32_t rs_, uint32_t rt_, uint16_t imm) {
    return (op << 26) | (rs_ << 21) | (rt_ << 16) | imm;
}
constexpr uint32_t bcond(uint32_t rs_, uint32_t sel, int16_t disp) {
    return (0x01 << 26) | (rs_ << 21) | (sel << 16) | uint16_t(disp);
}

struct Machine {
    Bus bus;
    CpuState cpu;
};
}  // namespace

// --- block 1: destinations -------------------------------------------------
TEST(destinations, branch_offset_from_delay_slot) {
    // branch at 0x100 with disp -1 must land at 0x100+4-4 = 0x100? No:
    // disp<<2 = -4, target = 0x104 - 4 = 0x100. Backwards branch to self.
    EXPECT_EQ(branch_target(0x100u, -1), 0x100u);
    EXPECT_EQ(branch_target(0x100u, +2), 0x10Cu);
    EXPECT_EQ(link_address(0x100u), 0x108u);   // past the delay slot
    EXPECT_EQ(jump_target(0xBEEF0000u), 0xBEEF0000u);
}

// --- block 2: window advance ------------------------------------------------
TEST(advance, sequential) {
    Window w{0x80010000u, 0x80010004u, false};
    const Window n = advance(w, false, 0);
    EXPECT_EQ(n.current_pc, 0x80010004u);
    EXPECT_EQ(n.next_pc, 0x80010008u);
    EXPECT_EQ(n.in_delay_slot, false);
}

TEST(advance, taken_branch_runs_slot_once_then_target) {
    Window w{0x100u, 0x104u, false};  // executing branch at 0x100
    const Window after_branch = advance(w, true, 0x200u);
    EXPECT_EQ(after_branch.current_pc, 0x104u);   // delay slot next...
    EXPECT_EQ(after_branch.in_delay_slot, true);
    EXPECT_EQ(after_branch.next_pc, 0x200u);      // ...then target
    const Window after_slot = advance(after_branch, false, 0xDEADu);
    EXPECT_EQ(after_slot.current_pc, 0x200u);     // slot executed exactly once
    EXPECT_EQ(after_slot.next_pc, 0x204u);
}

TEST(advance, untaken_branch_still_executes_slot_sequentially) {
    Window w{0x100u, 0x104u, false};
    const Window n = advance(w, false, 0x999u);
    EXPECT_EQ(n.current_pc, 0x104u);
    EXPECT_EQ(n.next_pc, 0x108u);
    EXPECT_EQ(n.in_delay_slot, false);
}

// --- block 3: branches/jumps -------------------------------------------------
TEST(branches, beq_bne_taken_not_taken) {
    Regs r;
    r.gpr[1] = 7;
    r.gpr[2] = 7;
    const FlowResult f = exec_branch(itype(0x04, 1, 2, 0x0010), r, 0x100u);
    EXPECT_EQ(f.flow, Flow::Taken);
    EXPECT_EQ(f.target, 0x144u);

    const FlowResult g = exec_branch(itype(0x05, 1, 2, 0x0010), r, 0x100u);
    EXPECT_EQ(g.flow, Flow::None);
}

TEST(branches, blez_bgtz_signed_zero_boundary) {
    Regs r;
    r.gpr[1] = 0;
    EXPECT_EQ(exec_branch(itype(0x06, 1, 0, 0), r, 0).flow, Flow::Taken);  // 0 <= 0
    EXPECT_EQ(exec_branch(itype(0x07, 1, 0, 0), r, 0).flow, Flow::None);   // !(0 > 0)
    r.gpr[1] = 0xFFFFFFFFu;  // -1
    EXPECT_EQ(exec_branch(itype(0x06, 1, 0, 0), r, 0).flow, Flow::Taken);
}

TEST(branches, regimm_bltz_bgez) {
    Regs r;
    r.gpr[3] = 0xFFFFFFFEu;  // -2
    EXPECT_EQ(exec_branch(bcond(3, 0x00, 1), r, 0x100u).flow, Flow::Taken);  // bltz
    EXPECT_EQ(exec_branch(bcond(3, 0x01, 1), r, 0x100u).flow, Flow::None);   // bgez
    r.gpr[3] = 5;
    EXPECT_EQ(exec_branch(bcond(3, 0x00, 1), r, 0x100u).flow, Flow::None);
    EXPECT_EQ(exec_branch(bcond(3, 0x01, 1), r, 0x100u).flow, Flow::Taken);
}

TEST(branches, j_target_uses_delay_slot_nibble) {
    Regs r;
    // j 0x80010010: index = target >> 2 (upper nibble comes from pc+4)
    const uint32_t instr = (0x02u << 26) | ((0x80010010u >> 2) & 0x03FFFFFFu);
    const FlowResult f = exec_branch(instr, r, 0x80010000u);
    EXPECT_EQ(f.target, 0x80010010u);
}

// --- block 4: muldiv ---------------------------------------------------------
TEST(muldiv, mult_signed_and_unsigned_split) {
    Regs r;
    r.gpr[1] = 0xFFFFFFFFu;  // -1 signed
    r.gpr[2] = 2;
    exec_muldiv(rtype(1, 2, 0, 0x18), r);              // mult -1 * 2 = -2
    EXPECT_EQ(r.lo, 0xFFFFFFFEu);
    EXPECT_EQ(r.hi, 0xFFFFFFFFu);                      // negative product
    exec_muldiv(rtype(1, 2, 0, 0x19), r);              // multu huge * 2
    EXPECT_EQ(r.hi, 1u);
    EXPECT_EQ(r.lo, 0xFFFFFFFEu);
}

TEST(muldiv, div_quotient_remainder_and_div0) {
    Regs r;
    r.gpr[1] = uint32_t(-7);
    r.gpr[2] = 2;
    exec_muldiv(rtype(1, 2, 0, 0x1A), r);              // div -7 / 2
    EXPECT_EQ(int32_t(r.lo), -3);                      // truncates toward zero
    EXPECT_EQ(int32_t(r.hi), -1);
    r.gpr[2] = 0;
    r.lo = 0xAAAAu;
    r.hi = 0xBBBBu;
    exec_muldiv(rtype(1, 2, 0, 0x1A), r);              // div by zero: unchanged
    EXPECT_EQ(r.lo, 0xAAAAu);
    EXPECT_EQ(r.hi, 0xBBBBu);
}

TEST(muldiv, move_costs_documented_latencies) {
    Regs r;
    r.gpr[1] = 9;
    EXPECT_EQ(exec_muldiv(rtype(1, 0, 3, 0x13), r), 1);      // mtlo
    EXPECT_EQ(exec_muldiv(rtype(0, 0, 4, 0x12), r), 1);      // mflo
    EXPECT_EQ(r.gpr[4], 9u);
    EXPECT_EQ(exec_muldiv(rtype(1, 1, 0, 0x19), r), 5);      // multu cost
    EXPECT_EQ(exec_muldiv(rtype(1, 1, 0, 0x1B), r), 37);     // divu cost
}

// --- block 5: full step integration ------------------------------------------
namespace {
void run(Machine& m, int n) {
    for (int i = 0; i < n && !m.cpu.halted; ++i) cpu_step(m.cpu, m.bus);
}
}  // namespace

TEST(step, alu_and_memory_program) {
    Machine m;
    const uint32_t prog[] = {
        itype(0x09, 0, 8, 0x1234),        // addiu $t0, $zero, 0x1234
        itype(0x2B, 0, 8, 0x400),         // sw $t0, 0x400($zero)
        itype(0x23, 0, 9, 0x400),         // lw $t1, 0x400($zero)
        rtype(0, 0, 0, 0x0C),             // syscall -> halt
    };
    m.bus.store_bytes(kProgramBase, reinterpret_cast<const uint8_t*>(prog),
                      sizeof prog);
    run(m, 20);
    EXPECT_TRUE(m.cpu.halted);
    EXPECT_EQ(m.cpu.regs.gpr[9], 0x1234u);  // stored, then loaded back through RAM
}

TEST(step, delay_slot_instruction_executes_exactly_once) {
    Machine m;
    const uint32_t base = kProgramBase;
    // addiu $t0,$zero,0      ; counter of slot executions
    // beq  $zero,$zero,+2    ; always taken, skips the poison addiu below
    // addiu $t0,$t0,1        ; DELAY SLOT — must run exactly once
    // addiu $t0,$t0,100      ; skipped by the branch (branch target lands here)
    // syscall
    const uint32_t prog[] = {
        itype(0x09, 0, 8, 0),
        itype(0x04, 0, 0, 0x0002),
        itype(0x09, 8, 8, 1),
        itype(0x09, 8, 8, 100),
        rtype(0, 0, 0, 0x0C),
    };
    static_assert(sizeof prog == 20, "");
    m.bus.store_bytes(base, reinterpret_cast<const uint8_t*>(prog), sizeof prog);
    run(m, 20);
    EXPECT_TRUE(m.cpu.halted);
    EXPECT_EQ(m.cpu.regs.gpr[8], 1u);  // slot ran once; skip target also once
}

TEST(step, jr_returns_through_delay_slot) {
    Machine m;
    // 0x00: jal func          ; func = 0x00 + 8*? place func at base+0x20
    // 0x04: nop (delay slot of jal)
    // 0x08: sw $t0,0x400(zero); executed AFTER return proves jr worked
    // 0x0C: syscall
    // ...
    // base+0x20: addiu $t0,$t0,5   <- function body
    // base+0x24: jr $ra
    const uint32_t base = kProgramBase;
    // base+0x28: addiu $t0,$t0,1   <- DELAY SLOT of jr: runs before return
    const uint32_t prog[] = {
        (0x03 << 26) | ((base + 0x20u) >> 2 & 0x03FFFFFFu),  // jal func
        0,                                                    // slot: nop
        itype(0x2B, 0, 8, 0x400),                             // sw $t0, 0x400
        rtype(0, 0, 0, 0x0C),                                 // syscall
        0, 0, 0, 0,
        itype(0x09, 8, 8, 5),                                 // func: $t0 += 5
        rtype(31, 0, 0, 0x08),                                // jr $ra
        itype(0x09, 8, 8, 1),                                 // slot: $t0 += 1
    };
    m.bus.store_bytes(base, reinterpret_cast<const uint8_t*>(prog), sizeof prog);
    run(m, 30);
    EXPECT_TRUE(m.cpu.halted);
    EXPECT_EQ(m.cpu.regs.gpr[8], 6u);  // body + slot both ran before return
    EXPECT_EQ(do_lw(m.bus, 0x400u), 6u);  // store after return saw final value
}

TEST(step, trace_format_matches_contract) {
    const std::string line = format_trace_line(0x80010004u, 0x24080001u, 42);
    EXPECT_EQ(line, "pc=80010004 op=24080001 cyc=42");
}
