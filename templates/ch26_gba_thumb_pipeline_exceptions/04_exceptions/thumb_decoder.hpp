#pragma once
#include <cstdint>

namespace thumb {

// Decoded Thumb halfword. Formats per LECTURE.md table; `imm` keeps the raw
// immediate field so consumers apply their own scaling/sign extension.
enum : uint8_t {
    kShift = 1,        // F1: LSL/LSR/ASR Rd,Rm,#imm5
    kAddSub = 2,       // F2: ADDS/SUBS Rd,Rn,Rm|#imm3
    kImmOp = 3,        // F3: MOV/CMP/ADD/SUB Rd,#imm8
    kAlu = 4,          // F4: ALU ops on two low registers
    kHiReg = 5,        // F5: ADD/CMP/MOV/BX high-register forms
    kPcRel = 6,        // F6: LDR Rd,[PC,#imm8*4]
    kPush = 7,         // PUSH {list[, LR]}
    kPop = 8,          // POP {list[, PC]}
    kCondBranch = 11,  // B<cond>, imm8 signed *2
    kBranch = 12,      // B, imm11 signed *2
    kBlFirst = 13,     // BL first halfword (high offset bits)
    kBlSecond = 14,    // BL second halfword (low offset bits)
};

// Format-1 shift ops / format-3 imm ops / format-5 hi-reg ops.
enum : uint8_t { kLSL = 0, kLSR = 1, kASR = 2 };
enum : uint8_t { kF3MOV = 0, kF3CMP = 1, kF3ADD = 2, kF3SUB = 3 };
enum : uint8_t { kF5ADD = 0, kF5CMP = 1, kF5MOV = 2, kBX = 3 };

struct Decoded {
    uint8_t fmt = 0;
    uint8_t op = 0;    // format-specific opcode
    uint8_t rd = 0;    // destination / low register
    uint8_t rs = 0;    // source / high register number (incl. H bit)
    uint8_t rn = 0;    // F2 register/immediate-3 field
    bool imm_form = false;  // F2 true when the field is an imm3
    uint16_t imm = 0;  // raw immediate field
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Formats 1-3: shifted immediates, add/sub, and the imm8 group.
inline bool decode_data(uint16_t hw, Decoded& d) {
    if ((hw >> 13) == 0 && ((hw >> 11) & 3) != 3) {           // F1
        d.fmt = kShift;
        d.op = (hw >> 11) & 3;                                // LSL/LSR/ASR
        d.imm = (hw >> 6) & 0x1F;
        d.rs = (hw >> 3) & 7;
        d.rd = hw & 7;
        return true;
    }
    if ((hw >> 11) == 0b00011) {                              // F2
        d.fmt = kAddSub;
        d.op = (hw >> 9) & 1;                                 // 1 = SUB
        d.imm_form = (hw >> 10) & 1;                          // I bit
        d.rn = (hw >> 6) & 7;                                 // Rn or imm3
        d.rs = (hw >> 3) & 7;
        d.rd = hw & 7;
        return true;
    }
    if ((hw >> 13) == 0b001) {                                // F3
        d.fmt = kImmOp;
        d.op = (hw >> 11) & 3;                                // MOV/CMP/ADD/SUB
        d.rd = (hw >> 8) & 7;
        d.imm = hw & 0xFF;
        return true;
    }
    return false;
}
//@LABS-STUB
inline bool decode_data(uint16_t hw, Decoded& d) {
    // TODO(1): recognize F1 (shift imm5), F2 (add/sub imm3/reg) and F3
    // (MOV/CMP/ADD/SUB #imm8) and fill the Decoded fields.
    (void)hw; (void)d;
    return false;
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Formats 4-5: register ALU ops and high-register/BX forms.
inline bool decode_reg(uint16_t hw, Decoded& d) {
    if ((hw >> 10) == 0b010000) {                             // F4
        d.fmt = kAlu;
        d.op = (hw >> 6) & 0xF;
        d.rs = (hw >> 3) & 7;
        d.rd = hw & 7;
        return true;
    }
    if ((hw >> 10) == 0b010001) {                             // F5
        d.fmt = kHiReg;
        d.op = (hw >> 8) & 3;                                 // ADD/CMP/MOV/BX
        const bool h1 = (hw >> 7) & 1;
        const bool h2 = (hw >> 6) & 1;
        d.rs = (h2 << 3) | ((hw >> 3) & 7);
        d.rd = (h1 << 3) | (hw & 7);
        return true;
    }
    return false;
}
//@LABS-STUB
inline bool decode_reg(uint16_t hw, Decoded& d) {
    // TODO(2): recognize F4 (010000 op Rs Rd) and F5 (010001 op H1 H2),
    // folding the H bits into rs/rd register numbers.
    (void)hw; (void)d;
    return false;
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// Format 6 (PC-relative load) and the stack forms PUSH/POP.
inline bool decode_mem(uint16_t hw, Decoded& d) {
    if ((hw >> 11) == 0b01001) {                              // F6
        d.fmt = kPcRel;
        d.rd = (hw >> 8) & 7;
        d.imm = hw & 0xFF;                                    // *4 at execute
        return true;
    }
    if ((hw & 0xFE00) == 0xB400) {                            // PUSH
        d.fmt = kPush;
        d.imm = hw & 0x1FF;                                   // bit8 = LR
        return true;
    }
    if ((hw & 0xFE00) == 0xBC00) {                            // POP
        d.fmt = kPop;
        d.imm = hw & 0x1FF;                                   // bit8 = PC
        return true;
    }
    return false;
}
//@LABS-STUB
inline bool decode_mem(uint16_t hw, Decoded& d) {
    // TODO(3): recognize F6 literal loads (01001 Rd imm8) and PUSH/POP
    // (1011 010L / 1011 110L lists).
    (void)hw; (void)d;
    return false;
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// Branch encodings: conditional B1, unconditional B2, and the two BL
// halfwords. branch_offset() sign-extends and scales the byte delta.
inline bool decode_branch(uint16_t hw, Decoded& d) {
    if ((hw >> 12) == 0b1101) {                               // B<cond>
        d.fmt = kCondBranch;
        d.op = (hw >> 8) & 0xF;                               // condition field
        d.imm = hw & 0xFF;
        return true;
    }
    if ((hw >> 11) == 0b11100) {                              // B
        d.fmt = kBranch;
        d.imm = hw & 0x7FF;
        return true;
    }
    if (((hw >> 11) & 0x1F) == 0b11110) {                     // BL first
        d.fmt = kBlFirst;
        d.imm = hw & 0x7FF;
        return true;
    }
    if ((hw >> 11) == 0b11111) {
        d.fmt = kBlSecond;
        d.imm = hw & 0x7FF;
        return true;
    }
    return false;
}

int32_t branch_offset(uint16_t imm, unsigned width_bits);

inline int32_t branch_offset(uint16_t imm, unsigned width_bits) {
    uint32_t v = imm;
    if (width_bits < 32 && (v >> (width_bits - 1)))
        v |= (0xFFFFFFFFu << width_bits);                     // sign extension
    return static_cast<int32_t>(v << 1);
}
//@LABS-STUB
inline bool decode_branch(uint16_t hw, Decoded& d) {
    // TODO(4): recognize B<cond> (1101 cond imm8), B (11100 imm11) and both
    // BL halfwords (1111 0/1 imm11).
    (void)hw; (void)d;
    return false;
}

inline int32_t branch_offset(uint16_t imm, unsigned width_bits) {
    // TODO(4): sign-extend the width-bit field, then scale by two (Thumb
    // instructions are halfword aligned).
    (void)imm; (void)width_bits;
    return 0;
}
//@LABS-END

// Convenience wrapper used by tests and executors: try each family in
// canonical order; first match wins.
inline bool decode(uint16_t hw, Decoded& d) {
    return decode_data(hw, d) || decode_reg(hw, d) ||
           decode_mem(hw, d) || decode_branch(hw, d);
}

inline Decoded decode(uint16_t hw) {
    Decoded d;
    decode(hw, d);
    return d;
}

 }  // namespace thumb
