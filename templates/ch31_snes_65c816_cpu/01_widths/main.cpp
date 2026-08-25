#define LABSTEST_MAIN
#include "labstest.hpp"

#include "cpu.hpp"

using snescpu::FC;
using snescpu::FM;
using snescpu::FN;
using snescpu::FX;

TEST(widths, pack_round_trip) {
    snescpu::Regs r;
    r.p = FN | FC | FX;
    EXPECT_EQ(snescpu::pack_p(r), FN | FC | FX);
    snescpu::Regs q;
    snescpu::unpack_p(q, 0xA5);
    EXPECT_EQ(snescpu::pack_p(q), 0xA5);
}

TEST(widths, sep_rep_native) {
    snescpu::Regs r;
    r.e = false;  // native mode
    r.p &= uint8_t(~(FM | FX));
    snescpu::sep(r, FM);
    EXPECT_TRUE(snescpu::a_is_8bit(r));
    EXPECT_FALSE(snescpu::xy_is_8bit(r));
    snescpu::sep(r, FX);
    EXPECT_TRUE(snescpu::xy_is_8bit(r));
    snescpu::rep(r, FM | FX);
    EXPECT_FALSE(snescpu::a_is_8bit(r));
    EXPECT_FALSE(snescpu::xy_is_8bit(r));
}

TEST(widths, emulation_forces_mx) {
    snescpu::Regs r;
    r.e = true;
    snescpu::rep(r, FM | FX);  // must NOT clear M/X in emulation
    EXPECT_TRUE(snescpu::a_is_8bit(r));
    EXPECT_TRUE(snescpu::xy_is_8bit(r));
    EXPECT_NE((snescpu::pack_p(r) & (FM | FX)), 0);
}

TEST(widths, xce_enter_emu_clears_index_high) {
    snescpu::Regs r;
    r.e = false;
    r.p |= FC;   // carry 1 -> XCE enters emulation
    r.x = 0xBEEF;
    r.y = 0x1234;
    snescpu::xce(r);
    EXPECT_TRUE(r.e);
    EXPECT_EQ((r.p & FC), 0);           // new carry = old E (was native=0)
    EXPECT_EQ(r.x, 0x00EF);             // high byte cleared
    EXPECT_EQ(r.y, 0x0034);
    EXPECT_TRUE(snescpu::a_is_8bit(r));
}

TEST(widths, xce_round_trip) {
    snescpu::Regs r;
    // Reset: E=1, C=0.
    snescpu::xce(r);  // E=0 (native), C=1
    EXPECT_FALSE(r.e);
    snescpu::rep(r, FM | FX);           // widen both in native
    EXPECT_FALSE(snescpu::a_is_8bit(r));
    snescpu::xce(r);  // back to emulation
    EXPECT_TRUE(r.e);
    EXPECT_EQ((r.p & FC), 0);           // new carry = old E (0)
    EXPECT_TRUE(snescpu::a_is_8bit(r)); // forced narrow again
}
