#define LABSTEST_MAIN
#include "labstest.hpp"

#include "boot_mini.hpp"

using namespace psx::r3000a;

namespace {

// Loads words as a BIOS image and resets the core on it.
void boot_with(BootMini& cpu, std::initializer_list<uint32_t> code) {
    std::vector<uint8_t> image(kBiosSize, 0xFF);
    uint32_t i = 0;
    for (uint32_t w : code) {
        image[i++] = w & 0xFF;
        image[i++] = (w >> 8) & 0xFF;
        image[i++] = (w >> 16) & 0xFF;
        image[i++] = (w >> 24) & 0xFF;
    }
    cpu.reset();
    cpu.bus.load_bios(image);
}

constexpr uint16_t kBranchSelf = 0xFFFF;  // `b .` — offset -1

constexpr uint32_t LUI(uint32_t rt, uint32_t imm16) {
    return (0x0Fu << 26) | (rt << 16) | imm16;
}
constexpr uint32_t ORI(uint32_t rs, uint32_t rt, uint32_t imm16) {
    return (0x0Du << 26) | (rs << 21) | (rt << 16) | imm16;
}
constexpr uint32_t ADDIU(uint32_t rs, uint32_t rt, uint32_t imm16) {
    return (0x09u << 26) | (rs << 21) | (rt << 16) | (imm16 & 0xFFFF);
}
constexpr uint32_t BEQ_SELF() { return (0x04u << 26) | 0xFFFFu; }
constexpr uint32_t BAL_PLUS1() {
    return (0x01u << 26) | (0x11u << 16) | 0x0001u;
}
constexpr uint32_t MFC0(uint32_t rt, uint32_t rd) {
    return 0x40000000u | (rt << 16) | (rd << 11);
}
constexpr uint32_t MTC0(uint32_t rt, uint32_t rd) {
    return 0x40000000u | (0x04u << 21) | (rt << 16) | (rd << 11);
}
constexpr uint32_t JR(uint32_t rs) { return (rs << 21) | 0x08u; }
constexpr uint32_t SW_(uint32_t rs, uint32_t rt, uint32_t imm16) {
    return (0x2Bu << 26) | (rs << 21) | (rt << 16) | (imm16 & 0xFFFF);
}
constexpr uint32_t LW_(uint32_t rs, uint32_t rt, uint32_t imm16) {
    return (0x23u << 26) | (rs << 21) | (rt << 16) | (imm16 & 0xFFFF);
}
constexpr uint32_t SYSCALL = 0x0000000Cu;
constexpr uint32_t RFE = 0x42000010u;

}  // namespace

TEST(bootmini, lui_ori_addiu_and_gpr_zero) {
    BootMini cpu;
    boot_with(cpu,
              {LUI(8, 0x1234), ORI(8, 9, 0x0056),
               ADDIU(9, 10, static_cast<uint16_t>(0xFFFCu)), BEQ_SELF(), 0});
    for (int i = 0; i < 3; ++i) EXPECT_FALSE(cpu.step().trapped);
    EXPECT_EQ(cpu.gpr[8], 0x12340000u);
    EXPECT_EQ(cpu.gpr[9], 0x12340056u);
    EXPECT_EQ(cpu.gpr[10], 0x12340052u);
}

TEST(bootmini, branch_delay_slot_executes_before_target) {
    BootMini cpu;
    const uint32_t base = kResetVector;
    // bal .+8 ; addiu $t0,$zero,7 ; halt
    boot_with(cpu, {BAL_PLUS1(), ADDIU(0, 8, 7), BEQ_SELF(), 0});
    cpu.step();                   // bal: ra = reset+8, next fetch is slot
    EXPECT_TRUE(cpu.in_delay_slot());
    EXPECT_EQ(cpu.pc, base + 4);  // delay slot address
    cpu.step();                   // slot instruction runs...
    EXPECT_EQ(cpu.gpr[8], 7u);    // ...before control reaches the target
    EXPECT_EQ(cpu.pc, base + 8);
}

TEST(bootmini, syscall_in_delay_slot_reports_bd_and_branch_epc) {
    BootMini cpu;
    // bal .+8 ; syscall — the fault fires inside the delay slot.
    boot_with(cpu, {BAL_PLUS1(), SYSCALL});
    cpu.step();
    StepEvent ev = cpu.step();
    EXPECT_TRUE(ev.trapped);
    EXPECT_EQ(ev.code, ExcCode::Syscall);
    EXPECT_TRUE(ev.bd);
    EXPECT_EQ(ev.epc, kResetVector);    // the BRANCH, not the slot
    EXPECT_EQ(ev.vector, 0xBFC00180u);  // BEV=1 after reset
    EXPECT_EQ((cpu.cop0.cause >> 2) & 0x1F,
              static_cast<uint32_t>(ExcCode::Syscall));
    EXPECT_NE(cpu.cop0.cause & CAUSE_BD, 0u);
}

TEST(bootmini, handler_return_sequence_via_jr_rfe) {
    BootMini cpu;
    const uint32_t base = kResetVector;
    // Main: syscall NOT in a slot, so EPC points at the syscall itself.
    // Handler bumps EPC past it and returns with `jr $k1; rfe`.
    {
        std::vector<uint8_t> image(kBiosSize, 0xFF);
        auto put = [&](uint32_t off,
                       std::initializer_list<uint32_t> code) {
            uint32_t i = off;
            for (uint32_t w : code) {
                image[i++] = w & 0xFF;
                image[i++] = (w >> 8) & 0xFF;
                image[i++] = (w >> 16) & 0xFF;
                image[i++] = (w >> 24) & 0xFF;
            }
        };
        put(0x00u, {SYSCALL,      // traps; EPC=BFC00000
                    0x00000000u,  // nop (resume point)
                    BEQ_SELF(), 0x00000000u});
        put(0x180u, {MFC0(27, COP0_EPC),  // mfc0 $k1,$14
                     ADDIU(27, 27, 4),    // skip the syscall
                     MTC0(27, COP0_EPC), JR(27),
                     RFE});  // rfe in the jump's delay slot
        cpu.reset();
        cpu.bus.load_bios(image);
    }

    StepEvent ev = cpu.step();  // syscall traps
    EXPECT_TRUE(ev.trapped);
    EXPECT_EQ(cpu.cop0.epc, base);
    EXPECT_EQ(cpu.cop0.sr & SR_IEC, 0u);  // pushed shadows: IRQs stay off

    for (int i = 0; i < 4; ++i) {  // mfc0 / addiu / mtc0 / jr
        EXPECT_FALSE(cpu.step().trapped);
    }
    EXPECT_TRUE(cpu.in_delay_slot());  // jr committed; rfe word is next
    EXPECT_EQ(cpu.pc, 0xBFC00190u);    // the retried jump's delay slot: rfe

    EXPECT_FALSE(cpu.step().trapped);  // rfe executes there
    EXPECT_EQ(cpu.cop0.sr & SR_BEV, SR_BEV);  // non-shadow bits survive
    EXPECT_EQ(cpu.cop0.epc, base + 4u);       // handler bumped EPC by 4
    EXPECT_EQ(cpu.pc, base + 4u);             // resumed past the syscall
}

TEST(bootmini, scratchpad_store_load_roundtrip) {
    BootMini cpu;
    boot_with(cpu,
              {LUI(9, 0x9F80), ORI(0, 8, 0xBEEF), SW_(9, 8, 4), LW_(9, 4, 4),
               BEQ_SELF(), 0});
    for (int i = 0; i < 4; ++i) EXPECT_FALSE(cpu.step().trapped);
    EXPECT_EQ(cpu.bus.scratchpad[4], 0xEFu);  // little-endian bytes
    EXPECT_EQ(cpu.bus.scratchpad[5], 0xBEu);
    EXPECT_EQ(cpu.gpr[4], 0x0000BEEFu);       // loaded back intact
}

TEST(bootmini, unaligned_lw_traps_with_address_error) {
    BootMini cpu;
    // lui $t1,0x8000; lw $t0,2($t1) -> RAM mirror, misaligned
    boot_with(cpu, {LUI(9, 0x8000), LW_(9, 8, 2)});
    EXPECT_FALSE(cpu.step().trapped);
    StepEvent ev = cpu.step();
    EXPECT_TRUE(ev.trapped);
    EXPECT_EQ(ev.code, ExcCode::AddressErrorLoad);
}

TEST(bootmini, reserved_opcode_traps_ri) {
    BootMini cpu;
    boot_with(cpu, {0xFC000000u /* invalid opcode */});
    StepEvent ev = cpu.step();
    EXPECT_TRUE(ev.trapped);
    EXPECT_EQ(ev.code, ExcCode::ReservedInstruction);
}
