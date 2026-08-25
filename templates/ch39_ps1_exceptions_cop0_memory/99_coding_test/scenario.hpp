#pragma once
// ch39 / 99_coding_test scenario — the UNSEEN sequence, assembled inline.
//
// Program narrative (all hand-assembled MIPS words):
//   - main enables interrupts (SR = BEV|Im8|IEc = 0x00400101),
//   - a timed "interrupt controller" asserts CAUSE.IP8 mid-stream,
//   - IRQ1 preempts the DELAY SLOT of `bal` (BD=1, EPC=branch): the handler
//     ACKs and retries the branch — the BD-aware return policy,
//   - a `syscall` follows (BD=0, EPC=syscall): the handler SKIPS it (EPC+4)
//     and re-enables IEc kernel-style before returning via `jr t8; rfe`,
//   - IRQ2 preempts a second mainline delay slot after everything unwinds,
//   - the mainline finishes by planting marker 0xC0DE in the scratchpad and
//     self-looping.
//
// Register contract: the handler keeps its return address in $t8 and never
// disturbs mainline t0/t1/t2/t3 beyond what the narrative stores. Note that
// a FULLY reentrant handler would also need to stack k0/k1/EPC before
// re-enabling interrupts (nocash: "there's no way to leave all registers
// intact"); this one deliberately keeps interrupt depth at one level.

#include <cstdint>
#include <vector>

#include "../91_challenge/boot_mini.hpp"

namespace psx::r3000a {

constexpr uint32_t kScenarioVector = 0xBFC00180u;

constexpr uint32_t LUI_(uint32_t rt, uint32_t imm) {
    return (0x0Fu << 26) | (rt << 16) | (imm & 0xFFFF);
}
constexpr uint32_t ORI_(uint32_t rs, uint32_t rt, uint32_t imm) {
    return (0x0Du << 26) | (rs << 21) | (rt << 16) | (imm & 0xFFFF);
}
constexpr uint32_t ADDIU_(uint32_t rs, uint32_t rt, uint32_t imm) {
    return (0x09u << 26) | (rs << 21) | (rt << 16) | (imm & 0xFFFF);
}
constexpr uint32_t BAL_PLUS1() { return (0x01u << 26) | (0x11u << 16) | 1; }
constexpr uint32_t BLTZ(uint32_t rs, int32_t off) {
    return (0x01u << 26) | (rs << 21) | (off & 0xFFFF);
}
constexpr uint32_t BEQ_(uint32_t rs, uint32_t rt, int32_t off) {
    return (0x04u << 26) | (rs << 21) | (rt << 16) | (off & 0xFFFF);
}
constexpr uint32_t SW_(uint32_t rs, uint32_t rt, uint32_t imm) {
    return (0x2Bu << 26) | (rs << 21) | (rt << 16) | (imm & 0xFFFF);
}
constexpr uint32_t MTC0_(uint32_t rt, uint32_t rd) {
    return 0x40000000u | (0x04u << 21) | (rt << 16) | (rd << 11);
}
constexpr uint32_t MFC0_(uint32_t rt, uint32_t rd) {
    return 0x40000000u | (rt << 16) | (rd << 11);
}
constexpr uint32_t JR_(uint32_t rs) { return (rs << 21) | 0x08u; }
constexpr uint32_t NOP = 0;
constexpr uint32_t SYSCALL_ = 0x0000000Cu;
constexpr uint32_t RFE_ = 0x42000010u;
constexpr uint32_t B_SELF() { return (0x04u << 26) | 0xFFFFu; }

// Timings verified against the reference implementation: each fake line
// must assert exactly when the named instruction is about to execute so
// BD/EPC land as documented.
constexpr long kIrqCycleFirstBalSlot = 5;    // preempts nop in 1st bal slot
constexpr long kIrqCycleSecondBalSlot = 40;  // preempts nop in 2nd bal slot

// Builds the full BIOS image (vector gap filled with 0xFF garbage that must
// never execute).
inline std::vector<uint8_t> build_scenario_image() {
    const uint32_t K0 = 26, K1 = 27;          // handler scratch
    const uint32_t T0 = 8, T2 = 10, T3 = 11, T8 = 24;
    std::vector<uint32_t> mem(kBiosSize / 4, 0xFFFFFFFFu);
    auto put = [&](uint32_t vaddr, uint32_t w) {
        // KSEG1 view -> physical -> BIOS image offset.
        const uint32_t off =
            (physical_address(vaddr) - kBiosBase) / 4;
        mem[off] = w;
    };

    // --- mainline ------------------------------------------------------
    put(0xBFC00000u, LUI_(T0, 0x0040));
    put(0xBFC00004u, ORI_(T0, T0, 0x0101));   // SR = BEV|Im8|IEc
    put(0xBFC00008u, MTC0_(T0, COP0_SR));
    put(0xBFC0000Cu, BAL_PLUS1());            // -> BFC00014
    put(0xBFC00010u, NOP);                    // IRQ1 preempts here
    put(0xBFC00014u, SYSCALL_);               // BD=0: handler skips it
    put(0xBFC00018u, BAL_PLUS1());            // -> BFC00020
    put(0xBFC0001Cu, NOP);                    // IRQ2 preempts here
    put(0xBFC00020u, LUI_(T3, 0x9F80));       // scratchpad (KSEG0)
    put(0xBFC00024u, ORI_(0, T2, 0xC0DE));
    put(0xBFC00028u, SW_(T3, T2, 64));        // marker -> sp[64]
    put(0xBFC0002Cu, B_SELF());
    put(0xBFC00030u, NOP);

    // --- general exception handler -------------------------------------
    put(kScenarioVector + 0x00u, MFC0_(K0, COP0_CAUSE));
    put(kScenarioVector + 0x04u, LUI_(T0, 0x9F80));
    put(kScenarioVector + 0x08u, SW_(T0, K0, 0));      // sp[0] = last CAUSE
    put(kScenarioVector + 0x0Cu, MFC0_(K1, COP0_EPC));
    put(kScenarioVector + 0x10u, SW_(T0, K1, 4));      // sp[4] = last EPC
    // CAUSE==0 <=> plain interrupt (ExcCode 0, BD=0) -> ACK + RETRY.
    put(kScenarioVector + 0x14u, BEQ_(K0, 0, 11));     // -> int_path @1C4
    put(kScenarioVector + 0x18u, NOP);
    // CAUSE<0 <=> BD=1 (fault in a delay slot) -> ACK + RETRY the branch.
    put(kScenarioVector + 0x1Cu, BLTZ(K0, 9));         // -> int_path @1C4
    put(kScenarioVector + 0x20u, NOP);
    // Fallthrough: syscall -> SKIP the faulting instruction.
    put(kScenarioVector + 0x24u, ADDIU_(K1, K1, 4));
    put(kScenarioVector + 0x28u, MTC0_(K1, COP0_EPC));
    // Kernel-style re-enable: allow deeper interrupts while handling.
    put(kScenarioVector + 0x2Cu, MFC0_(T3, COP0_SR));
    put(kScenarioVector + 0x30u, ORI_(T3, T3, 0x0001));  // IEc := 1
    put(kScenarioVector + 0x34u, MTC0_(T3, COP0_SR));
    put(kScenarioVector + 0x38u, MFC0_(T8, COP0_EPC));
    put(kScenarioVector + 0x3Cu, JR_(T8));
    put(kScenarioVector + 0x40u, RFE_);
    // int_path: acknowledge the line and retry the preempted instruction.
    put(kScenarioVector + 0x44u, MTC0_(0, COP0_CAUSE));  // clear IP bits 8-9
    put(kScenarioVector + 0x48u, MFC0_(T8, COP0_EPC));   // EPC untouched
    put(kScenarioVector + 0x4Cu, JR_(T8));
    put(kScenarioVector + 0x50u, RFE_);

    std::vector<uint8_t> image(kBiosSize, 0xFF);
    for (uint32_t i = 0; i < mem.size(); ++i) {
        image[i * 4 + 0] = mem[i] & 0xFF;
        image[i * 4 + 1] = (mem[i] >> 8) & 0xFF;
        image[i * 4 + 2] = (mem[i] >> 16) & 0xFF;
        image[i * 4 + 3] = (mem[i] >> 24) & 0xFF;
    }
    return image;
}

}  // namespace psx::r3000a
