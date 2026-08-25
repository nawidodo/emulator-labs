#define LABSTEST_MAIN
#include "labstest.hpp"
#include "exception.hpp"

using namespace psx::r3000a;

TEST(exception, vector_selects_by_bev) {
    EXPECT_EQ(general_vector(false), 0x80000080u);  // RAM/KSEG0 (PSX default)
    EXPECT_EQ(general_vector(true), 0xBFC00180u);   // ROM/KSEG1 boot vectors
}

TEST(exception, syscall_direct_records_epc_and_code) {
    Cop0 c;
    c.reset();  // BEV=1
    ExceptionResult r = take_exception(
        &c, {0xBFC0000C, false, std::nullopt, ExcCode::Syscall});
    EXPECT_EQ(r.vector, 0xBFC00180u);
    EXPECT_EQ(c.epc, 0xBFC0000Cu);
    EXPECT_FALSE((c.cause & CAUSE_BD) != 0);
    EXPECT_EQ((c.cause & CAUSE_EXCCODE_MASK) >> 2,
              static_cast<uint32_t>(ExcCode::Syscall));
    EXPECT_EQ(r.epc, c.epc);
}

TEST(exception, interrupt_in_delay_slot_points_at_branch) {
    // Faulting instruction sits in the delay slot of `bal` at 0xBFC00008.
    Cop0 c;
    c.reset();
    ExceptionResult r =
        take_exception(&c, {0xBFC0000C, true, uint32_t{0xBFC00008},
                           ExcCode::Interrupt});
    EXPECT_EQ(c.epc, 0xBFC00008u);              // branch address, not slot
    EXPECT_TRUE((c.cause & CAUSE_BD) != 0);     // handler must add 8 to skip
    EXPECT_EQ((c.cause & CAUSE_EXCCODE_MASK) >> 2,
              static_cast<uint32_t>(ExcCode::Interrupt));
    EXPECT_EQ(r.vector, 0xBFC00180u);
}

TEST(exception, entry_pushes_sr_shadows) {
    Cop0 c;
    c.sr = SR_BEV | SR_IM_MASK | SR_IEC;  // kernel, IRQ enabled, BEV=1
    take_exception(&c, {0x80000100, false, std::nullopt, ExcCode::Overflow});
    EXPECT_EQ(c.sr & SR_IEC, 0u);
    EXPECT_EQ(c.sr & SR_KUC, 0u);
    EXPECT_EQ(c.sr & SR_IEP, SR_IEP);          // old current -> previous
    EXPECT_EQ(apply_rfe(c.sr) & SR_IEC, SR_IEC);
}

TEST(exception, bev_zero_vector_used_when_bev_cleared) {
    Cop0 c;
    c.reset();
    c.sr &= ~SR_BEV;  // what the real BIOS does after copying handlers to RAM
    ExceptionResult r =
        take_exception(&c, {0x80000060, false, std::nullopt, ExcCode::Syscall});
    EXPECT_EQ(r.vector, 0x80000080u);
}

TEST(exception, coprocessor_fault_records_ce_field) {
    Cop0 c;
    c.reset();
    take_exception(&c, {0x00001000, false, std::nullopt,
                       ExcCode::CoprocessorUnusable, 2});
    EXPECT_EQ((c.cause & CAUSE_CE_MASK) >> 28, 2u);
}

TEST(exception, rfe_return_sequence_recovers_original_state) {
    // Canonical return: bump EPC if needed, `jr epc; rfe`.
    Cop0 c;
    const uint32_t original = SR_BEV | SR_IM_MASK | SR_IEC;
    c.sr = original;
    take_exception(&c, {0xBFC00010, true, uint32_t{0xBFC00008},
                       ExcCode::Syscall});
    EXPECT_NE(c.sr, original);
    c.sr = apply_rfe(c.sr);
    EXPECT_EQ(c.sr & SR_IEC, original & SR_IEC);
    EXPECT_EQ(c.epc, 0xBFC00008u);  // handler decides: +8 skips branch+slot
}
