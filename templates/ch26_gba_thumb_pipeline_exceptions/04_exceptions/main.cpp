#define LABSTEST_MAIN
#include "labstest.hpp"
#include "exc_cpu.hpp"

using namespace exc;
using exc::kFIQ;
using exc::kIRQ;
using exc::kSvc;
using exc::kUser;

static constexpr uint32_t arm_mov_imm(uint32_t rd, uint32_t imm8) {
    return 0xE3A00000u | (rd << 12) | imm8;
}
static constexpr uint32_t arm_swi(uint32_t num) {
    return 0xEF000000u | num;
}

TEST(exceptions, mode_bits_are_distinct) {
    // The documented CPU modes table: user/fiq/irq/svc encodings.
    EXPECT_EQ(kUser, 0x10u);
    EXPECT_EQ(kFIQ, 0x11u);
    EXPECT_EQ(kIRQ, 0x12u);
    EXPECT_EQ(kSvc, 0x13u);
    ExceptionCpu cpu;
    EXPECT_EQ(cpu.cpsr & 0x1F, kUser);
}

TEST(exceptions, swi_enters_supervisor_and_return_restores) {
    ExceptionCpu cpu;
    cpu.write32(0x00, arm_mov_imm(0, 1));    // mov r0,#1 (user state)
    cpu.write32(0x04, arm_swi(0x123456));    // swi handler call
    cpu.write32(0x08, arm_mov_imm(1, 2));    // vector entry: mov r1,#2
    cpu.cpsr |= FLAG_Z;                      // a flag the SWI must preserve
    cpu.step();                              // MOV
    const uint32_t user_cpsr = cpu.cpsr;
    cpu.step();                              // SWI
    EXPECT_EQ(cpu.mode, kSvc);
    EXPECT_EQ((cpu.cpsr & 0x1F), kSvc);
    EXPECT_EQ(cpu.r[15], 0x08u);             // vectored
    EXPECT_EQ(cpu.svc.r14, 0x08u);           // instr_addr + 4
    EXPECT_EQ(cpu.svc.spsr, user_cpsr);      // full CPSR snapshot
    // Supervisor work done, then return with MOVS PC, LR.
    cpu.exception_return(0);
    EXPECT_EQ(cpu.mode, kUser);
    EXPECT_EQ(cpu.cpsr, user_cpsr);          // flags + mode restored
    EXPECT_EQ(cpu.r[15], 0x08u);             // MOVS PC, LR
}

TEST(exceptions, irq_masking_and_vector) {
    ExceptionCpu cpu;
    cpu.r[15] = 0x100;                       // "running" somewhere
    EXPECT_TRUE(cpu.irq_line());             // unmasked: recognized
    EXPECT_EQ(cpu.mode, kIRQ);
    EXPECT_EQ(cpu.r[15], 0x18u);             // IRQ vector
    EXPECT_NE((cpu.cpsr & (1u << 7)), 0u);   // I set by entry
    EXPECT_EQ(cpu.irq.r14, 0x104u);          // interrupted slot + 4
    EXPECT_FALSE(cpu.irq_line());            // now masked: pending only
    cpu.exception_return(4);                 // SUBS PC, LR, #4
    EXPECT_EQ(cpu.mode, kUser);
    EXPECT_EQ(cpu.r[15], 0x100u);            // resume where recognized
    EXPECT_EQ((cpu.cpsr & (1u << 7)), 0u);   // mask restored from SPSR
    EXPECT_TRUE(cpu.irq_line());             // interruptible again
}

TEST(exceptions, fiq_banks_r8_through_r12) {
    ExceptionCpu cpu;
    for (int i = 0; i < 5; ++i) cpu.r[8 + i] = static_cast<uint32_t>(0xA0 + i);
    cpu.r[15] = 0x200;
    EXPECT_TRUE(cpu.fiq_line());
    EXPECT_EQ(cpu.mode, kFIQ);
    for (int i = 0; i < 5; ++i)
        EXPECT_EQ(cpu.r[8 + i], 0u);         // FIQ-private bank visible
    // Handler scribbles on its private registers.
    for (int i = 0; i < 5; ++i)
        cpu.r[8 + i] = static_cast<uint32_t>(0xF0 + i);
    cpu.exception_return(4);
    EXPECT_EQ(cpu.mode, kUser);
    for (int i = 0; i < 5; ++i)
        EXPECT_EQ(cpu.r[8 + i],
                  static_cast<uint32_t>(0xA0 + i));  // caller's regs intact
    EXPECT_EQ(cpu.fiq_shadow[0], 0xF0u);     // FIQ values kept in shadow
}

TEST(hidden, exceptions_hidden_nested_entry) {
    // IRQ taken while running in SVC after an SWI: nested banking must not
    // clobber the SVC link register.
    ExceptionCpu cpu;
    cpu.write32(0x00, arm_swi(2));
    cpu.write32(0x08, arm_mov_imm(2, 9));    // svc-mode work
    cpu.step();                              // SWI -> SVC @8
    EXPECT_EQ(cpu.mode, kSvc);
    const uint32_t svc_cpsr = cpu.cpsr;
    cpu.step();                              // MOV inside handler
    EXPECT_TRUE(cpu.irq_line());             // device interrupt arrives
    EXPECT_EQ(cpu.mode, kIRQ);
    EXPECT_EQ(cpu.irq.r14, 0x10u);           // next fetch addr + 4
    EXPECT_EQ(cpu.svc.r14, 0x04u);           // SVC link untouched
    cpu.exception_return(4);
    EXPECT_EQ(cpu.mode, kSvc);
    EXPECT_EQ(cpu.cpsr, svc_cpsr);
}
