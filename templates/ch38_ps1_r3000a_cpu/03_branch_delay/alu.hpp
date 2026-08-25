#pragma once
#include <cstdint>

namespace psx::r3000a {

// R3000A register file. $zero is hardwired: set() discards writes to index 0
// so the executor never needs a special case per instruction.
struct Regs {
    uint32_t gpr[32] = {};
    uint32_t hi = 0;
    uint32_t lo = 0;

    void set(uint32_t idx, uint32_t val) {
        if (idx != 0) gpr[idx] = val;
    }
    uint32_t get(uint32_t idx) const { return gpr[idx]; }
};

// ---- MIPS I instruction field extraction --------------------------------
constexpr inline uint32_t opcode(uint32_t i) { return i >> 26; }
constexpr inline uint32_t rs(uint32_t i)     { return (i >> 21) & 31u; }
constexpr inline uint32_t rt(uint32_t i)     { return (i >> 16) & 31u; }
constexpr inline uint32_t rd(uint32_t i)     { return (i >> 11) & 31u; }
constexpr inline uint32_t shamt(uint32_t i)  { return (i >> 6) & 31u; }
constexpr inline uint32_t funct(uint32_t i)  { return i & 63u; }
constexpr inline uint16_t imm16(uint32_t i)  { return static_cast<uint16_t>(i); }
// Arithmetic/address immediates sign-extend; logical immediates zero-extend.
constexpr inline uint32_t sext16(uint32_t i) {
    return static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(imm16(i))));
}
constexpr inline uint32_t zext16(uint32_t i) { return imm16(i); }

//@LABS-BEGIN 1
//@LABS-SOLUTION
// R-type three-register ALU group (SPECIAL, op=0). ADDU/SUBU deliberately
// wrap — on R3000A only ADD/SUB/I-variants trap overflow, and PS1 code
// almost exclusively uses the wrapping forms.
inline bool exec_alu_r(uint32_t instr, Regs& r) {
    switch (funct(instr)) {
        case 0x21: r.set(rd(instr), r.get(rs(instr)) + r.get(rt(instr))); return true;   // addu
        case 0x23: r.set(rd(instr), r.get(rs(instr)) - r.get(rt(instr))); return true;   // subu
        case 0x24: r.set(rd(instr), r.get(rs(instr)) & r.get(rt(instr))); return true;   // and
        case 0x25: r.set(rd(instr), r.get(rs(instr)) | r.get(rt(instr))); return true;   // or
        case 0x26: r.set(rd(instr), r.get(rs(instr)) ^ r.get(rt(instr))); return true;   // xor
        case 0x27: r.set(rd(instr), ~(r.get(rs(instr)) | r.get(rt(instr)))); return true;// nor
        case 0x2A: r.set(rd(instr), static_cast<int32_t>(r.get(rs(instr))) <
                                       static_cast<int32_t>(r.get(rt(instr))) ? 1u : 0u);
                   return true;                                                          // slt
        case 0x2B: r.set(rd(instr), r.get(rs(instr)) < r.get(rt(instr)) ? 1u : 0u);
                   return true;                                                          // sltu
        default:   return false;
    }
}
//@LABS-STUB
// TODO(1): implement the R-type ALU group (SPECIAL funct codes):
// addu=0x21 subu=0x23 and=0x24 or=0x25 xor=0x26 nor=0x27 slt=0x2A sltu=0x2B.
// Return false for anything unhandled. Remember $zero writes are discarded
// by Regs::set, and slt compares SIGNED while sltu compares unsigned.
inline bool exec_alu_r(uint32_t instr, Regs& r) {
    (void)instr;
    (void)r;
    return false;  // nothing decoded yet -> suite runs RED
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// I-type ALU group. addiu sign-extends; andi/ori/xori ZERO-extend; lui is
// just rt = imm << 16 and ignores rs entirely.
inline bool exec_alu_i(uint32_t instr, Regs& r) {
    switch (opcode(instr)) {
        case 0x09: r.set(rt(instr), r.get(rs(instr)) + sext16(instr)); return true;      // addiu
        case 0x0C: r.set(rt(instr), r.get(rs(instr)) & zext16(instr)); return true;      // andi
        case 0x0D: r.set(rt(instr), r.get(rs(instr)) | zext16(instr)); return true;      // ori
        case 0x0E: r.set(rt(instr), r.get(rs(instr)) ^ zext16(instr)); return true;      // xori
        case 0x0A: r.set(rt(instr), static_cast<int32_t>(r.get(rs(instr))) <
                                       static_cast<int32_t>(sext16(instr)) ? 1u : 0u);
                   return true;                                                          // slti
        case 0x0B: r.set(rt(instr), r.get(rs(instr)) < sext16(instr) ? 1u : 0u);
                   return true;                                                          // sltiu
        case 0x0F: r.set(rt(instr), imm16(instr) << 16); return true;                    // lui
        default:   return false;
    }
}
//@LABS-STUB
// TODO(2): implement the I-type ALU group:
// addiu=0x09 slti=0x0A sltiu=0x0B andi=0x0C ori=0x0D xori=0x0E lui=0x0F.
// Sign-extension applies to addiu/slti/sltiu comparisons; logical ops use
// zero-extension; lui shifts the immediate into the top halfword.
inline bool exec_alu_i(uint32_t instr, Regs& r) {
    (void)instr;
    (void)r;
    return false;  // TODO(2)
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// Shift group (SPECIAL). Immediate forms take shamt; variable forms mask the
// rs register value mod 64 by hardware convention — in practice values are
// always < 32 because assemblers reject larger shamt, and rs&31 == rs for
// any realistic input, but we follow the spec exactly with & 31.
inline bool exec_shifts(uint32_t instr, Regs& r) {
    const uint32_t val = r.get(rt(instr));
    switch (funct(instr)) {
        case 0x00: r.set(rd(instr), val << shamt(instr)); return true;                   // sll
        case 0x02: r.set(rd(instr), val >> shamt(instr)); return true;                   // srl
        case 0x03: r.set(rd(instr), static_cast<uint32_t>(
                       static_cast<int32_t>(val) >> shamt(instr))); return true;         // sra
        case 0x04: r.set(rd(instr), val << (r.get(rs(instr)) & 31u)); return true;       // sllv
        case 0x06: r.set(rd(instr), val >> (r.get(rs(instr)) & 31u)); return true;       // srlv
        case 0x07: r.set(rd(instr), static_cast<uint32_t>(
                       static_cast<int32_t>(val) >> (r.get(rs(instr)) & 31u)));
                   return true;                                                          // srav
        default:   return false;
    }
}
//@LABS-STUB
// TODO(3): implement the shift group (SPECIAL funct codes):
// sll=0x00 srl=0x02 sra=0x03 sllv=0x04 srlv=0x06 srav=0x07.
// sll/srl/sra shift by the shamt field; *v forms shift by rs&31.
// sra/srav shift arithmetically (sign-filling).
inline bool exec_shifts(uint32_t instr, Regs& r) {
    (void)instr;
    (void)r;
    return false;  // TODO(3)
}
//@LABS-END

}  // namespace psx::r3000a
