#pragma once
#include <cstdint>
#include "conditions.hpp"
#include "shifter.hpp"

// Debugging exercise core. Same architecture as 05_branches' ArmCpu, but
// five defects are seeded in the @LABS-STUB sides below. Find them all;
// document each in bug-report.md (bug / root cause / first divergence /
// fix / regression test). See DEBUGGING.md for observed symptoms.

namespace arm {

enum DpOp : uint32_t {
    kAND = 0, kEOR, kSUB, kRSB, kADD, kADC, kSBC, kRSC,
    kTST, kTEQ, kCMP, kCMN, kORR, kMOV, kBIC, kMVN,
};
inline bool op_is_arithmetic(DpOp op) {
    return (op >= kSUB && op <= kRSC) || op == kCMP || op == kCMN;
}
inline bool op_writes_result(DpOp op) { return !(op >= kTST && op <= kCMN); }

struct DebugCpu {
    static constexpr uint32_t kMemSize = 64 * 1024;
    uint8_t mem[kMemSize] = {};
    uint32_t r[16] = {};
    uint32_t cpsr = 0;
    bool shifter_carry = false;
    bool shifter_carry_valid = false;

    static bool flag_n(uint32_t v) { return v >> 31; }
    static bool flag_z(uint32_t v) { return v == 0; }
    bool c_flag() const { return cpsr & FLAG_C; }
    void set_nzcv(bool n, bool z, bool c, bool v) {
        cpsr = (cpsr & ~0xF0000000u) | (n ? FLAG_N : 0) | (z ? FLAG_Z : 0) |
               (c ? FLAG_C : 0) | (v ? FLAG_V : 0);
    }

    uint32_t read32(uint32_t addr) const {
        const uint32_t m = kMemSize - 1, a = addr & m;
        return mem[a] | (mem[(a + 1) & m] << 8) | (mem[(a + 2) & m] << 16) |
               (mem[(a + 3) & m] << 24);
    }
    void write32(uint32_t addr, uint32_t v) {
        const uint32_t m = kMemSize - 1, a = addr & m;
        mem[a] = v; mem[(a + 1) & m] = v >> 8;
        mem[(a + 2) & m] = v >> 16; mem[(a + 3) & m] = v >> 24;
    }

    // --- Bug 1 lives here: the shifter's carry handling. ---
    //@LABS-BEGIN 1
//@LABS-SOLUTION
    uint32_t read_operand2(uint32_t instr) {
        const uint32_t rm = instr & 0xF;
        if (instr & (1 << 25)) {
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
        const uint32_t rm = instr & 0xF;
        if (instr & (1 << 25)) {
            const uint32_t rot = (instr >> 8) & 0xF;
            const auto res = shift_ror(2 * rot, instr & 0xFF, c_flag());
            shifter_carry = res.carry_out;
            // TODO(1): seeded bug — find and fix.
            shifter_carry_valid = true;   // BUG: rotate-0 immediates now
                                          // clobber C with a stale value
            return res.value;
        }
        const uint32_t type = (instr >> 5) & 3;
        const auto res = shift_imm(type, (instr >> 7) & 0x1F, rm, c_flag());
        shifter_carry = res.carry_out;
        shifter_carry_valid = true;
        return res.value;
    }
//@LABS-END

    // --- Bug 2 lives here: ADC/SBC carry-in. ---
    //@LABS-BEGIN 2
//@LABS-SOLUTION
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
        const uint64_t sum =
            static_cast<uint64_t>(a) + b;   // BUG: carry-in dropped
        // TODO(2): seeded bug — find and fix.
        const uint32_t res = static_cast<uint32_t>(sum);
        return {res, (sum >> 32) != 0,
               ((~(a ^ b) & (a ^ res)) >> 31) != 0};
    }
//@LABS-END
    unsigned exec_arith_cycles(uint32_t instr) {
        const DpOp op = static_cast<DpOp>((instr >> 21) & 0xF);
        (void)op;
        return 1;
    }
    // --- Bug 3 lives here: SBC borrow polarity. ---
    //@LABS-BEGIN 3
//@LABS-SOLUTION
    uint32_t exec_arithmetic(uint32_t instr) {
        const DpOp op = static_cast<DpOp>((instr >> 21) & 0xF);
        const bool s = instr & (1 << 20);
        const uint32_t rn = (instr >> 16) & 0xF;
        const uint32_t rd = (instr >> 12) & 0xF;
        const uint32_t b = read_operand2(instr);
        AddOut out{};
        switch (op) {
        case kSUB: case kCMP: out = add_with_carry(r[rn], ~b, true);     break;
        case kRSB:            out = add_with_carry(~r[rn], b, true);     break;
        case kADD: case kCMN: out = add_with_carry(r[rn], b, false);     break;
        case kADC:            out = add_with_carry(r[rn], b, c_flag());  break;
        case kSBC:            out = add_with_carry(r[rn], ~b, c_flag()); break;
        case kRSC:            out = add_with_carry(~r[rn], b, c_flag()); break;
        default:              break;
        }
        if (s)
            set_nzcv(flag_n(out.value), flag_z(out.value), out.carry,
                     out.overflow);
        if (op_writes_result(op)) r[rd] = out.value;
        return out.value;
    }
//@LABS-STUB
    uint32_t exec_arithmetic(uint32_t instr) {
        const DpOp op = static_cast<DpOp>((instr >> 21) & 0xF);
        const bool s = instr & (1 << 20);
        const uint32_t rn = (instr >> 16) & 0xF;
        const uint32_t rd = (instr >> 12) & 0xF;
        const uint32_t b = read_operand2(instr);
        AddOut out{};
        switch (op) {
        case kSUB: case kCMP: out = add_with_carry(r[rn], ~b, true);     break;
        case kRSB:            out = add_with_carry(~r[rn], b, true);     break;
        case kADD: case kCMN: out = add_with_carry(r[rn], b, false);     break;
        case kADC:            out = add_with_carry(r[rn], b, c_flag());  break;
        // TODO(3): seeded bug — find and fix.
        case kSBC:            out = add_with_carry(r[rn], ~b, !c_flag()); break;  // BUG: inverted carry-in
        case kRSC:            out = add_with_carry(~r[rn], b, !c_flag()); break;  // BUG: inverted carry-in
        default:              break;
        }
        if (s)
            set_nzcv(flag_n(out.value), flag_z(out.value), out.carry,
                     out.overflow);
        if (op_writes_result(op)) r[rd] = out.value;
        return out.value;
    }
//@LABS-END

    // --- Bugs 4 and 5 live in exec() below. ---
    //@LABS-BEGIN 4
//@LABS-SOLUTION
    unsigned exec(uint32_t instr, uint32_t pc) {
        const DpOp op = static_cast<DpOp>((instr >> 21) & 0xF);
        if ((instr & 0x0C000000) == 0x00000000) {
            exec_arithmetic(instr);
            return 1;
        }
        // Logical group: C sourced from shifter only when shifting happened.
        const bool s = instr & (1 << 20);
        const uint32_t rd = (instr >> 12) & 0xF;
        const uint32_t b = read_operand2(instr);
        uint32_t res = 0;
        switch (op) {
        case kAND: case kTST: res = r[(instr >> 16) & 0xF] & b; break;
        case kEOR: case kTEQ: res = r[(instr >> 16) & 0xF] ^ b; break;
        case kORR:            res = r[(instr >> 16) & 0xF] | b; break;
        case kMOV:            res = b;                          break;
        case kBIC:            res = r[(instr >> 16) & 0xF] & ~b; break;
        case kMVN:            res = ~b;                         break;
        default:              break;
        }
        if (s)
            set_nzcv(flag_n(res), flag_z(res),
                     shifter_carry_valid ? shifter_carry : c_flag(),
                     cpsr & FLAG_V);
        if (op_writes_result(op)) r[rd] = res;
        return 1;
    }
//@LABS-STUB
    unsigned exec(uint32_t instr, uint32_t pc) {
        const DpOp op = static_cast<DpOp>((instr >> 21) & 0xF);
        if ((instr & 0x0C000000) == 0x00000000) {
            exec_arithmetic(instr);
            return 1;
        }
        const bool s = instr & (1 << 20);
        const uint32_t rd = (instr >> 12) & 0xF;
        const uint32_t b = read_operand2(instr);
        uint32_t res = 0;
        switch (op) {
        case kAND: case kTST: res = r[(instr >> 16) & 0xF] & b; break;
        case kEOR: case kTEQ: res = r[(instr >> 16) & 0xF] ^ b; break;
        case kORR:            res = r[(instr >> 16) & 0xF] | b; break;
        case kMOV:            res = b;                          break;
        case kBIC:            res = r[(instr >> 16) & 0xF] & ~b; break;
        case kMVN:            res = ~b;                         break;
        default:              break;
        }
        if (s)
            set_nzcv(flag_n(res), flag_z(res),
                     shifter_carry_valid ? shifter_carry : c_flag(),
                     cpsr & FLAG_V);
        r[rd] = res;   // BUG: TST/TEQ/CMP/CMN must not write back
        // TODO(4): seeded bug — find and fix.
        return 1;
    }
//@LABS-END

    //@LABS-BEGIN 5
//@LABS-SOLUTION
    unsigned step() {
        const uint32_t pc = r[15];
        const uint32_t instr = read32(pc);
        uint32_t next = pc + 4;
        unsigned cycles = 1;
        if ((instr >> 28) == 0xE || cond_pass(instr >> 28, cpsr)) {
            if ((instr & 0x0E000000) == 0x0A000000) {          // B/BL
                const bool link = instr & (1 << 24);
                uint32_t imm = instr & 0x00FFFFFF;
                if (imm & 0x00800000) imm |= 0xFF000000u;
                next = pc + 8 + (static_cast<int32_t>(imm << 2));
                if (link) r[14] = pc + 4;
                cycles = 3;
            } else {
                cycles = exec(instr, pc);
            }
        }
        r[15] = next;
        return cycles;
    }
//@LABS-STUB
    unsigned step() {
        const uint32_t pc = r[15];
        const uint32_t instr = read32(pc);
        uint32_t next = pc + 4;
        unsigned cycles = 1;
        if ((instr >> 28) == 0xE || cond_pass(instr >> 28, cpsr)) {
            if ((instr & 0x0E000000) == 0x0A000000) {          // B/BL
                const bool link = instr & (1 << 24);
                uint32_t imm = instr & 0x00FFFFFF;
                if (imm & 0x00800000) imm |= 0xFF000000u;
                next = pc + 8 + (static_cast<int32_t>(imm << 2));
                if (link) r[14] = pc + 8;   // BUG: LR off by 4
                // TODO(5): seeded bug — find and fix.
                cycles = 3;
            } else {
                cycles = exec(instr, pc);
            }
        }
        r[15] = next;
        return cycles;
    }
//@LABS-END
};

}  // namespace arm
