#define LABSTEST_MAIN
#include "labstest.hpp"
#include "regimm_al.hpp"

using namespace psx::r3000a;

namespace {
constexpr uint32_t bcond(uint32_t rs_, uint32_t sel, int16_t disp) {
    return (0x01 << 26) | (rs_ << 21) | (sel << 16) | uint16_t(disp);
}
}  // namespace

TEST(bltzal, taken_links_and_branches) {
    Regs r;
    r.gpr[4] = 0xFFFFFFFEu;  // -2
    FlowResult f;
    EXPECT_TRUE(exec_regimm_link(bcond(4, 0x10, 3), r, 0x100u, f));
    EXPECT_EQ(r.gpr[31], 0x108u);
    EXPECT_EQ(f.flow, Flow::Taken);
    EXPECT_EQ(f.target, 0x110u);
}

TEST(bltzal, not_taken_still_links) {
    Regs r;
    r.gpr[4] = 7;  // positive: bltzal not taken
    FlowResult f;
    EXPECT_TRUE(exec_regimm_link(bcond(4, 0x10, 3), r, 0x100u, f));
    EXPECT_EQ(r.gpr[31], 0x108u);   // MIPS I links unconditionally
    EXPECT_EQ(f.flow, Flow::None);
}

TEST(bgezal, zero_is_nonnegative) {
    Regs r;
    r.gpr[4] = 0;
    FlowResult f;
    EXPECT_TRUE(exec_regimm_link(bcond(4, 0x11, -2), r, 0x200u, f));
    EXPECT_EQ(f.flow, Flow::Taken);
    EXPECT_EQ(f.target, 0x200u + 4u - 8u);
}

TEST(regimm_link, rejects_other_encodings) {
    Regs r;
    FlowResult f;
    // plain bltz (rt=0x00) belongs to the base set, not this executor
    EXPECT_FALSE(exec_regimm_link(bcond(4, 0x00, 1), r, 0x100u, f));
    // wrong opcode entirely
    EXPECT_FALSE(exec_regimm_link(0x20000000u, r, 0x100u, f));
}
