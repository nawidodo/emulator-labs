#define LABSTEST_MAIN
#include "labstest.hpp"

#include "coding_test.hpp"
#include "scenario.hpp"

using namespace psx::r3000a;

namespace {

NestedCpu booted_scenario() {
    NestedCpu cpu;
    cpu.reset();
    cpu.bus.load_bios(build_scenario_image());
    cpu.irq_cycle_a = kIrqCycleFirstBalSlot;
    cpu.irq_cycle_b = kIrqCycleSecondBalSlot;
    return cpu;
}

}  // namespace

TEST(codingtest, deliverable_needs_ip_im_and_iec) {
    NestedCpu cpu;
    cpu.reset();  // BEV only: no IP, no Im, no IEc
    EXPECT_FALSE(cpu.deliverable());
    cpu.cop0.cause |= kCauseIpSwInterrupt;  // pending, but masked+disabled
    EXPECT_FALSE(cpu.deliverable());
    cpu.cop0.sr |= kSrImBit8;               // unmasked, still disabled
    EXPECT_FALSE(cpu.deliverable());
    cpu.cop0.sr |= SR_IEC;                  // now deliverable
    EXPECT_TRUE(cpu.deliverable());
    cpu.cop0.sr &= ~kSrImBit8;              // mask wins over enable
    EXPECT_FALSE(cpu.deliverable());
}

TEST(codingtest, irq1_hits_delay_slot_with_bd_and_branch_epc) {
    NestedCpu cpu = booted_scenario();
    StepEvent ev{};
    for (long c = 0; c < kIrqCycleFirstBalSlot; ++c) ev = cpu.step_irq();
    // The preempted instruction is the nop in bal's delay slot.
    EXPECT_TRUE(ev.trapped);
    EXPECT_EQ(ev.code, ExcCode::Interrupt);
    EXPECT_TRUE(ev.bd);
    EXPECT_EQ(ev.epc, 0xBFC0000Cu);
    EXPECT_EQ(ev.vector, 0xBFC00180u);
    EXPECT_NE(cpu.cop0.cause & CAUSE_BD, 0u);
    EXPECT_EQ(cpu.cop0.sr & SR_IEC, 0u);  // pushed into the handler
}

TEST(codingtest, handler_acks_and_retries_the_branch) {
    NestedCpu cpu = booted_scenario();
    for (long c = 0; c < kIrqCycleFirstBalSlot; ++c) (void)cpu.step_irq();
    // Handler prologue + dispatch: ack clears IP8...
    for (int i = 0; i < 10; ++i) (void)cpu.step_irq();  // through mtc0 $0,$13
    EXPECT_EQ(cpu.cop0.cause & kCauseIpSwInterrupt, 0u);
    // ...and the rfe returns to the BRANCH, which re-executes.
    for (int i = 0; i < 3; ++i) (void)cpu.step_irq();   // mfc0 t8/jr/rfe
    EXPECT_EQ(cpu.pc, 0xBFC0000Cu);                     // the branch again
    const StepEvent ev = cpu.step_irq();                // retried bal
    EXPECT_FALSE(ev.trapped);
    const StepEvent slot_ev = cpu.step_irq();           // its delay slot runs
    EXPECT_FALSE(slot_ev.trapped);                      // no refire after ack
    EXPECT_EQ(cpu.pc, 0xBFC00014u);                     // into the syscall
}

TEST(codingtest, syscall_is_skipped_exactly_once) {
    NestedCpu cpu = booted_scenario();
    // Run past the first exception and the retried bal into the syscall.
    // Cycles 1..20 leave the CPU about to execute the syscall (the interrupt
    // delivery itself consumes a cycle without retiring an instruction).
    for (long c = 0; c < kIrqCycleFirstBalSlot + 15; ++c)
        (void)cpu.step_irq();
    StepEvent ev = cpu.step_irq();  // this IS the syscall step
    EXPECT_TRUE(ev.trapped);
    EXPECT_FALSE(ev.bd);
    EXPECT_EQ(ev.epc, 0xBFC00014u);
    EXPECT_EQ((cpu.cop0.cause >> 2) & 0x1F,
              static_cast<uint32_t>(ExcCode::Syscall));

    // Handler bumps EPC by 4: resumption lands AFTER the syscall.
    for (int i = 0; i < 17; ++i) (void)cpu.step_irq();
    EXPECT_EQ(cpu.pc, 0xBFC00018u);
    EXPECT_EQ(cpu.cop0.epc, 0xBFC00018u);
}

TEST(codingtest, full_run_terminates_in_halt_loop_with_marker) {
    NestedCpu cpu = booted_scenario();
    for (long c = 0; c < 96; ++c) (void)cpu.step_irq();
    EXPECT_EQ(cpu.pc, 0xBFC0002Cu);  // self-loop (`b .`)
    uint32_t cause = 0, epc = 0, marker = 0;
    bus_read(&cpu.bus, 0x9F800000u, &cause);
    bus_read(&cpu.bus, 0x9F800004u, &epc);
    bus_read(&cpu.bus, 0x9F800040u, &marker);
    EXPECT_EQ(cause, 0x80000100u);  // BD|Int with IP8 still pending at entry
    EXPECT_EQ(epc, 0xBFC00018u);
    EXPECT_EQ(marker, 0xC0DEu);
}
