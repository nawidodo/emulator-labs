#define LABSTEST_MAIN
#include "labstest.hpp"

#include <optional>

#include "exception.hpp"

using namespace psx::r3000a;

namespace {

Cop0 base_cop0() {
    Cop0 c{};
    c.sr = SR_IEC | SR_KUC;   // user mode, interrupts on: a real entry
    return c;                 // must push BOTH into the shadow slots
}

}  // namespace

// --- BUG(1): delay-slot faults must record the BRANCH in EPC ------------

TEST(debug39, delay_slot_epc_points_at_branch) {
    Cop0 c = base_cop0();
    ExceptionResult r = take_exception(
        &c, {0x00001000, true, std::optional<uint32_t>(0x00000FF8),
             ExcCode::Syscall});
    EXPECT_EQ(r.epc, 0x00000FF8);      // BUG(1) writes 0x00001000
    EXPECT_TRUE(r.cause & CAUSE_BD);
}

TEST(debug39, direct_fault_records_own_pc) {
    Cop0 c = base_cop0();
    ExceptionResult r =
        take_exception(&c, {0x80015000, false, std::nullopt,
                            ExcCode::Syscall});
    EXPECT_EQ(r.epc, 0x80015000);
    EXPECT_FALSE(r.cause & CAUSE_BD);
}

// --- BUG(2): SR kernel shadow must be pushed on entry -------------------

TEST(debug39, entry_pushes_kernel_shadow) {
    Cop0 c = base_cop0();
    const uint32_t sr_before = c.sr;
    take_exception(&c, {0x80015000, false, std::nullopt, ExcCode::Syscall});
    // After a correct push the old IEc/KUc move into the shadow slots and
    // the machine enters kernel mode with interrupts masked.
    EXPECT_NE(c.sr, sr_before);        // BUG(2) leaves SR untouched
    EXPECT_TRUE((c.sr & (SR_KUC | SR_IEC)) == 0);
}

TEST(debug39, exccode_and_vector_still_correct) {
    Cop0 bev_on = base_cop0();
    bev_on.sr |= SR_BEV;
    ExceptionResult r = take_exception(
        &bev_on, {0x00001000, false, std::nullopt, ExcCode::Overflow});
    EXPECT_EQ(r.vector, 0xBFC00180u);
    EXPECT_EQ((r.cause & CAUSE_EXCCODE_MASK) >> 2,
              static_cast<uint32_t>(ExcCode::Overflow));
}
