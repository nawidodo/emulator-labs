#pragma once
// ch39 / 01_cop0_regs — COP0 register file of the R3000A (PSX-SPX "COP0" tables).
//
// The PlayStation CPU exposes four system-control registers we model here:
//   SR    (r12) — kernel/user + interrupt-enable triple shadow, IRQ mask, BEV
//   CAUSE (r13) — why the last exception happened (ExcCode, BD, IP)
//   EPC   (r14) — address to resume at (branch address when CAUSE.BD=1)
//   PRID  (r15) — processor revision (1 on CXD8530BQ/CQ, 2 on CXD8606CQ)
//
// Reference: nocash PSX-SPX, https://problemkaputt.de/psx-spx.htm#cpucoprocessor0

#include <cstdint>

namespace psx::r3000a {

// --- COP0 register indices -------------------------------------------------
enum : uint32_t {
    COP0_SR = 12,
    COP0_CAUSE = 13,
    COP0_EPC = 14,
    COP0_PRID = 15,
};

// --- SR (status register) bit positions ------------------------------------
enum SrBits : uint32_t {
    SR_IEC = 1u << 0,   // current interrupt enable
    SR_KUC = 1u << 1,   // current mode (0=kernel, 1=user)
    SR_IEP = 1u << 2,   // previous interrupt enable
    SR_KUP = 1u << 3,   // previous mode
    SR_IEO = 1u << 4,   // old interrupt enable
    SR_KUO = 1u << 5,   // old mode
    SR_IM_MASK = 0xFFu << 8,  // per-source interrupt mask (IRQ0..IRQ7)
    SR_ISC = 1u << 16,  // isolate data cache (scratchpad mode)
    SR_SWC = 1u << 17,  // swap icache/dcache
    SR_BEV = 1u << 22,  // boot exception vectors (0=RAM/KSEG0, 1=ROM/KSEG1)
    SR_CU2 = 1u << 30,  // COP2 (GTE) usable
};
// --- CAUSE bit positions ---------------------------------------------------
enum CauseBits : uint32_t {
    CAUSE_EXCCODE_MASK = 0x1Fu << 2,
    CAUSE_IP_MASK = 0xFFu << 8,  // interrupt pending; bits 8-9 are software R/W
    CAUSE_CE_MASK = 0x3u << 28,  // coprocessor unit number (COP faults)
    CAUSE_BD = 1u << 31,         // faulting instruction was in a branch delay slot
};

// ExcCode values (CAUSE bits 6:2). Only the ones the PSX can produce matter;
// TLB codes exist in the architecture but the PSX has no TLB.
enum class ExcCode : uint32_t {
    Interrupt = 0,
    TlbModification = 1,
    TlbLoad = 2,
    TlbStore = 3,
    AddressErrorLoad = 4,
    AddressErrorStore = 5,
    BusErrorInstruction = 6,
    BusErrorData = 7,
    Syscall = 8,
    Breakpoint = 9,
    ReservedInstruction = 10,
    CoprocessorUnusable = 11,
    Overflow = 12,
};

struct Cop0 {
    uint32_t sr = 0;
    uint32_t cause = 0;
    uint32_t epc = 0;
    uint32_t prid = 1;  // CXD8530BQ/CXD8530CQ revision; newer CPUs report 2

    void reset() {
        // Reset state: kernel mode, interrupts disabled, boot vectors in ROM.
        sr = SR_BEV;
        cause = 0;
        epc = 0;
    }

    uint32_t read(uint32_t reg) const;
    void write(uint32_t reg, uint32_t value);
};
// Register access side. PRID is read-only; CAUSE accepts writes to its
// software IP bits (8-9) only, everything else reads back what was stored.
inline uint32_t Cop0::read(uint32_t reg) const {
    switch (reg) {
        case COP0_SR: return sr;
        case COP0_CAUSE: return cause;
        case COP0_EPC: return epc;
        case COP0_PRID: return prid;
        default: return 0;  // unmapped COP0 regs read as garbage/zero here
    }
}

inline void Cop0::write(uint32_t reg, uint32_t value) {
    switch (reg) {
        case COP0_SR: sr = value; break;
        case COP0_CAUSE:
            // Only bits 8-9 are writable (software interrupt request).
            cause = (cause & ~(CAUSE_IP_MASK & 0x300u)) | (value & 0x300u);
            break;
        case COP0_EPC: epc = value; break;
        default: break;  // PRID and friends ignore writes on real silicon
    }
}


//@LABS-BEGIN 1
//@LABS-SOLUTION
// Encoding of a COP0 move: COP0 opcode (0b010000) with rs selecting the
// direction — 0x00 = MFC0 (read COP0 into GPR rt), 0x04 = MTC0 (write GPR rt
// into COP0 rd). Example: `mfc0 $k0, $13` -> 0x401A6800.
inline constexpr uint32_t encode_mfc0(uint32_t rt, uint32_t rd) {
    return 0x40000000u | (rt << 16) | (rd << 11);
}
inline constexpr uint32_t encode_mtc0(uint32_t rt, uint32_t rd) {
    return 0x40000000u | (0x04u << 21) | (rt << 16) | (rd << 11);
}
// `rfe` — whole-word constant, no fields: 0x42000010.
inline constexpr uint32_t kRfeEncoding = 0x42000010u;

struct Cop0Move {
    bool is_mtc0;
    uint32_t gpr;  // rt
    uint32_t reg;  // rd (COP0 register index)
};

// Decode any COP0 word that is a register move (rs field 0 or 4). Returns
// false for other COP0 ops (TLBR..., RFE) so callers can dispatch separately.
inline bool decode_cop0_move(uint32_t word, Cop0Move* out) {
    if ((word >> 26) != 0x10) return false;
    const uint32_t rs = (word >> 21) & 0x1F;
    if (rs != 0x00 && rs != 0x04) return false;
    out->is_mtc0 = (rs == 0x04);
    out->gpr = (word >> 16) & 0x1F;
    out->reg = (word >> 11) & 0x1F;
    return true;
}
//@LABS-STUB
// TODO(1): encode/decode MFC0/MTC0 words.
//   MFC0: 0x40000000 | (rt << 16) | (rd << 11);  MTC0: same but rs=0x04.
inline constexpr uint32_t encode_mfc0(uint32_t rt, uint32_t rd) {
    (void)rt; (void)rd;
    return 0;  // wrong on purpose
}
inline constexpr uint32_t encode_mtc0(uint32_t rt, uint32_t rd) {
    (void)rt; (void)rd;
    return 0;  // wrong on purpose
}
inline constexpr uint32_t kRfeEncoding = 0u;  // TODO(1): 0x42000010

struct Cop0Move {
    bool is_mtc0;
    uint32_t gpr;
    uint32_t reg;
};

inline bool decode_cop0_move(uint32_t word, Cop0Move* out) {
    (void)word; (void)out;
    return false;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Architectural state update performed by EVERY exception entry (R3000A
// double-shadow push): the current pair slides down into previous, previous
// into old. Entering the handler therefore lands in kernel mode with
// interrupts disabled, and two levels of nesting stay recoverable.
inline uint32_t push_sr_on_exception(uint32_t sr) {
    const uint32_t cur = sr & (SR_IEC | SR_KUC);
    const uint32_t prev = sr & (SR_IEP | SR_KUP);
    sr &= ~(cur | prev);              // clear current+previous slots
    sr |= cur << 2;                   // current  -> previous
    sr |= prev << 2;                  // previous -> old
    return sr;                        // IEc=KUc=0: kernel, IRQs off
}
//@LABS-STUB
// TODO(2): slide SR current/previous shadow pairs down one level on exception
// entry (current->previous, previous->old) and clear the current pair.
inline uint32_t push_sr_on_exception(uint32_t sr) {
    (void)sr;
    return sr;  // wrong on purpose: no push happens
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// `rfe` (Restore From Exception): undo ONE level of the push — previous pair
// back into current, old pair back into previous. Old level is discarded.
// NOTE: rfe does NOT jump anywhere; the handler must `jr` to EPC itself with
// rfe in the jump's delay slot.
inline uint32_t apply_rfe(uint32_t sr) {
    const uint32_t prev = sr & (SR_IEP | SR_KUP);
    const uint32_t old = sr & (SR_IEO | SR_KUO);
    sr &= ~(prev | old);              // clear previous+old slots
    sr |= prev >> 2;                  // previous -> current
    sr |= old << 0;                   // old      -> previous
    return sr;
}
//@LABS-STUB
// TODO(3): restore one SR shadow level (previous->current, old->previous),
// leaving the current pair's new contents intact.
inline uint32_t apply_rfe(uint32_t sr) {
    (void)sr;
    return sr;  // wrong on purpose
}
//@LABS-END

}  // namespace psx::r3000a
