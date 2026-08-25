#define LABSTEST_MAIN
#include "labstest.hpp"
#include "cop0.hpp"

using namespace psx::r3000a;

TEST(cop0, mfc0_encoding) {
    // `mfc0 $k0, $13` (read CAUSE into r26): rs=0(MFC0), rt=26, rd=13.
    EXPECT_EQ(encode_mfc0(26, 13), 0x401A6800u);
}

TEST(cop0, mtc0_encoding) {
    // `mtc0 $t0, $12` (write r8 into SR): rs=4(MTC0), rt=8, rd=12.
    EXPECT_EQ(encode_mtc0(8, 12), 0x40886000u);
    // MTC0 differs from MFC0 only in the rs field bits 24:21.
    EXPECT_EQ(encode_mtc0(26, 13), encode_mfc0(26, 13) | (0x04u << 21));
}

TEST(cop0, rfe_encoding) {
    EXPECT_EQ(kRfeEncoding, 0x42000010u);
}

TEST(cop0, move_decode_roundtrip) {
    Cop0Move m{};
    EXPECT_TRUE(decode_cop0_move(encode_mfc0(26, 13), &m));
    EXPECT_FALSE(m.is_mtc0);
    EXPECT_EQ(m.gpr, 26u);
    EXPECT_EQ(m.reg, 13u);

    EXPECT_TRUE(decode_cop0_move(encode_mtc0(8, 12), &m));
    EXPECT_TRUE(m.is_mtc0);
    EXPECT_EQ(m.gpr, 8u);
    EXPECT_EQ(m.reg, 12u);

    // RFE and TLB ops are not register moves.
    EXPECT_FALSE(decode_cop0_move(kRfeEncoding, &m));
    EXPECT_FALSE(decode_cop0_move(0x42000018u /* eret-ish junk */, &m));
    EXPECT_FALSE(decode_cop0_move(0x0000000Cu /* syscall */, &m));
}

TEST(cop0, sr_push_enters_kernel_with_irqs_off) {
    // Kernel mode + interrupts enabled + IM for IRQ2 + BEV=1.
    const uint32_t sr = SR_BEV | SR_IM_MASK | SR_IEC;
    const uint32_t pushed = push_sr_on_exception(sr);
    EXPECT_EQ(pushed & SR_IEC, 0u);           // interrupts disabled now
    EXPECT_EQ(pushed & SR_KUC, 0u);           // kernel mode
    EXPECT_EQ(pushed & SR_IEP, SR_IEP);       // old IEc slid into IEp
    EXPECT_EQ((pushed >> 22) & 1, 1u);        // BEV untouched by the push
}

TEST(cop0, sr_push_is_two_levels_deep) {
    // Start with previous pair already occupied (nested handler context).
    const uint32_t sr = SR_IEC | SR_IEP | SR_IEO | SR_KUC | SR_KUP | SR_KUO;
    const uint32_t pushed = push_sr_on_exception(sr);
    EXPECT_EQ(pushed & (SR_IEC | SR_KUC), 0u);
    EXPECT_EQ(pushed & (SR_IEP | SR_KUP), (SR_IEC | SR_KUC) << 2);
    EXPECT_EQ(pushed & (SR_IEO | SR_KUO), (SR_IEP | SR_KUP) << 2);
}

TEST(cop0, rfe_pops_one_level) {
    uint32_t sr = SR_BEV | SR_IM_MASK | SR_IEC;  // kernel, IRQs enabled
    sr = push_sr_on_exception(sr);
    EXPECT_EQ(sr & SR_IEC, 0u);                  // handler: IRQs off
    sr = apply_rfe(sr);
    EXPECT_EQ(sr & SR_IEC, SR_IEC);              // back to enabled
    EXPECT_EQ(sr & (SR_IEP | SR_KUP), 0u);       // previous slot emptied
    EXPECT_EQ((sr >> 22) & 1, 1u);               // BEV survives everything
}

TEST(cop0, push_then_rfe_roundtrip) {
    const uint32_t sr = SR_BEV | SR_CU2 | SR_IM_MASK | SR_IEC | SR_KUC;
    EXPECT_EQ(apply_rfe(push_sr_on_exception(sr)),
              (sr & ~(SR_IEP | SR_KUP)));  // prev pair zeroed by the pop
}

TEST(cop0, prid_readonly_and_reset_state) {
    Cop0 c;
    c.reset();
    EXPECT_EQ(c.read(COP0_PRID), 1u);   // CXD8530BQ/CQ reports revision 1
    c.write(COP0_PRID, 0xDEADBEEFu);    // writes to PRID are ignored
    EXPECT_EQ(c.read(COP0_PRID), 1u);

    EXPECT_EQ(c.sr & SR_BEV, SR_BEV);   // reset: boot vectors in ROM/KSEG1
    EXPECT_EQ(c.sr & SR_IEC, 0u);       // interrupts off until kernel sets IEc
    EXPECT_EQ(c.cause, 0u);
}

TEST(cop0, cause_software_ip_bits_writable) {
    Cop0 c;
    c.reset();
    c.write(COP0_CAUSE, 0x100u);        // request software interrupt IP8
    EXPECT_EQ(c.read(COP0_CAUSE) & 0x300u, 0x100u);
    // ExcCode field is not writable through mtc0 on this model.
    c.write(COP0_CAUSE, static_cast<uint32_t>(ExcCode::Syscall) << 2);
    EXPECT_EQ(c.read(COP0_CAUSE) & CAUSE_EXCCODE_MASK, 0u);
}
