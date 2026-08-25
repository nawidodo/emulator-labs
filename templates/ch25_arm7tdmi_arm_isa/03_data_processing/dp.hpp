#pragma once
#include <cstdint>
#include "conditions.hpp"
#include "shifter.hpp"

namespace arm {

// ARM data-processing opcodes (bits 24-21).
enum DpOp : uint32_t {
    kAND = 0, kEOR, kSUB, kRSB, kADD, kADC, kSBC, kRSC,
    kTST, kTEQ, kCMP, kCMN, kORR, kMOV, kBIC, kMVN,
};

inline bool op_is_arithmetic(DpOp op) {
    return (op >= kSUB && op <= kRSC) || op == kCMP || op == kCMN;
}
inline bool op_writes_result(DpOp op) {  // false for TST/TEQ/CMP/CMN
    return !(op >= kTST && op <= kCMN);
}

struct DpCpu {
    uint32_t r[16] = {};
    uint32_t cpsr = 0;

    // Carry produced by the operand-2 shifter during the current instruction.
    // Kept strictly separate from the ALU carry — conflating them is the
    // classic ARM emulation bug this chapter exists to kill.
    bool shifter_carry = false;
    // True when the shifter actually performed work this instruction
    // (immediate with rotate==0 leaves C untouched in hardware).
    bool shifter_carry_valid = false;

    static bool flag_n(uint32_t v) { return v >> 31; }
    static bool flag_z(uint32_t v) { return v == 0; }

    void set_nzcv(bool n, bool z, bool c, bool v) {
        cpsr = (cpsr & ~0xF0000000u) | (n ? FLAG_N : 0) | (z ? FLAG_Z : 0) |
               (c ? FLAG_C : 0) | (v ? FLAG_V : 0);
    }
    bool c_flag() const { return cpsr & FLAG_C; }

    //@LABS-BEGIN 1
//@LABS-SOLUTION
    // Resolve operand 2 of a DP instruction: immediate-rotate (I=1,
    // imm8 ror 2*rot) or shifted register (I=0). Fills shifter_carry /
    // shifter_carry_valid and returns the operand value.
    uint32_t read_operand2(uint32_t instr) {
        const uint32_t rm = instr & 0xF;
        if (instr & (1 << 25)) {                       // immediate rotate
            const uint32_t rot = (instr >> 8) & 0xF;
            const auto res = shift_ror(2 * rot, instr & 0xFF, c_flag());
            shifter_carry = res.carry_out;
            shifter_carry_valid = rot != 0;
            return res.value;
        }
        const uint32_t type = (instr >> 5) & 3;
        const auto res = (instr & (1 << 4))
                             ? shift_reg(type, r[(instr >> 8) & 0xF], r[rm],
                                         c_flag())
                             : shift_imm(type, (instr >> 7) & 0x1F, r[rm],
                                         c_flag());
        shifter_carry = res.carry_out;
        shifter_carry_valid = true;
        return res.value;
    }
//@LABS-STUB
    uint32_t read_operand2(uint32_t instr) {
        // TODO(1): decode operand 2. Immediate form: ror(imm8, 2*rot),
        // carry valid only when rot != 0. Register form: shift by imm5, or
        // by Rs when bit4 is set.
        (void)instr;
        shifter_carry = false;
        shifter_carry_valid = false;
        return 0;
    }
//@LABS-END

    //@LABS-BEGIN 2
//@LABS-SOLUTION
    // 33-bit add returning result, ALU carry-out and signed overflow.
    struct AddOut { uint32_t value; bool carry; bool overflow; };
    static AddOut add_with_carry(uint32_t a, uint32_t b, bool c_in) {
        const uint64_t sum =
            static_cast<uint64_t>(a) + b + (c_in ? 1u : 0u);
        const uint32_t res = static_cast<uint32_t>(sum);
        return {res, (sum >> 32) != 0,
               ((~(a ^ b) & (a ^ res)) >> 31) != 0};
    }
//@LABS-STUB
    struct AddOut { uint32_t value; bool carry; bool overflow; };
    static AddOut add_with_carry(uint32_t a, uint32_t b, bool c_in) {
        // TODO(2): full-width add; overflow = same-sign operands giving a
        // different-sign result.
        (void)a; (void)b; (void)c_in;
        return {a + b, false, false};
    }
//@LABS-END

    //@LABS-BEGIN 3
//@LABS-SOLUTION
    // Arithmetic group with S semantics: C comes from the ALU (inverted-
    // borrow convention for subtracts), V from signed overflow. Returns
    // the result for writeback by the caller.
    uint32_t exec_arithmetic(DpOp op, uint32_t a, uint32_t b, bool s) {
        AddOut out{};
        switch (op) {
        case kSUB: case kCMP: out = add_with_carry(a, ~b, true);     break;
        case kRSB:            out = add_with_carry(~a, b, true);     break;
        case kADD: case kCMN: out = add_with_carry(a, b, false);     break;
        case kADC:            out = add_with_carry(a, b, c_flag());  break;
        case kSBC:            out = add_with_carry(a, ~b, c_flag()); break;
        case kRSC:            out = add_with_carry(~a, b, c_flag()); break;
        default:              out = add_with_carry(a, b, false);     break;
        }
        if (s) set_nzcv(flag_n(out.value), flag_z(out.value), out.carry,
                        out.overflow);
        return out.value;
    }
//@LABS-STUB
    uint32_t exec_arithmetic(DpOp op, uint32_t a, uint32_t b, bool s) {
        // TODO(3): SUB/RSB/ADD/ADC/SBC/RSC (+ CMP/CMN aliases). Subtracts
        // are adds of the inverted operand; C = NOT borrow.
        (void)op; (void)a; (void)b; (void)s;
        return 0;
    }
//@LABS-END

    //@LABS-BEGIN 4
//@LABS-SOLUTION
    // Logical group with S semantics: C comes from the *shifter* carry
    // (unchanged when no shifting happened), V is never modified by logical
    // ops. Returns the result for writeback.
    uint32_t exec_logical(DpOp op, uint32_t a, uint32_t b, bool s) {
        const bool v_old = cpsr & FLAG_V;
        uint32_t res = 0;
        switch (op) {
        case kAND: case kTST: res = a & b;   break;
        case kEOR: case kTEQ: res = a ^ b;   break;
        case kORR:            res = a | b;   break;
        case kMOV:            res = b;       break;
        case kBIC:            res = a & ~b;  break;
        case kMVN:            res = ~b;      break;
        default:              res = 0;       break;
        }
        if (s)
            set_nzcv(flag_n(res), flag_z(res),
                     shifter_carry_valid ? shifter_carry : c_flag(), v_old);
        return res;
    }
//@LABS-STUB
    uint32_t exec_logical(DpOp op, uint32_t a, uint32_t b, bool s) {
        // TODO(4): AND/EOR/ORR/MOV/BIC/MVN (+ TST/TEQ aliases). With S set,
        // C must come from shifter_carry, never from any ALU state, and V
        // must survive unchanged.
        (void)op; (void)a; (void)b; (void)s;
        return 0;
    }
//@LABS-END

    //@LABS-BEGIN 5
//@LABS-SOLUTION
    // Execute one DP instruction word. Assumes cond already checked.
    // Returns cycles consumed (1S on this simple model).
    unsigned exec_dp(uint32_t instr) {
        const DpOp op = static_cast<DpOp>((instr >> 21) & 0xF);
        const bool s = instr & (1 << 20);
        const uint32_t rn = (instr >> 16) & 0xF;
        const uint32_t rd = (instr >> 12) & 0xF;
        const uint32_t op2 = read_operand2(instr);

        if (!op_is_arithmetic(op)) {
            const uint32_t res =
                exec_logical(op, (op == kMOV || op == kMVN) ? 0 : r[rn], op2,
                             s);
            if (op_writes_result(op)) r[rd] = res;
        } else {
            const uint32_t res = exec_arithmetic(op, r[rn], op2, s);
            if (op_writes_result(op)) r[rd] = res;
        }
        return 1;
    }
//@LABS-STUB
    unsigned exec_dp(uint32_t instr) {
        // TODO(5): dispatch: decode opcode/S/Rd, route arithmetic vs logical
        // groups, suppress writeback for TST/TEQ/CMP/CMN.
        (void)instr;
        return 1;
    }
//@LABS-END
};

}  // namespace arm
