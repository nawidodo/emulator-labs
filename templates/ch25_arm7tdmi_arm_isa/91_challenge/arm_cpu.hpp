#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include "conditions.hpp"
#include "shifter.hpp"

namespace arm {

enum DpOp : uint32_t {
    kAND = 0, kEOR, kSUB, kRSB, kADD, kADC, kSBC, kRSC,
    kTST, kTEQ, kCMP, kCMN, kORR, kMOV, kBIC, kMVN,
};
inline bool op_is_arithmetic(DpOp op) {
    return (op >= kSUB && op <= kRSC) || op == kCMP || op == kCMN;
}
inline bool op_writes_result(DpOp op) { return !(op >= kTST && op <= kCMN); }

// Combined mini ARM core used by the chapter's headless runner and its
// branch exercises: data processing, single load/store, B/BL/BX.
struct ArmCpu {
    static constexpr uint32_t kMemSize = 64 * 1024;
    uint8_t mem[kMemSize] = {};
    uint32_t r[16] = {};
    uint32_t cpsr = 0x00000010;          // user mode bits
    bool thumb = false;                  // T bit tracked by BX
    bool shifter_carry = false;
    bool shifter_carry_valid = false;

    static bool flag_n(uint32_t v) { return v >> 31; }
    static bool flag_z(uint32_t v) { return v == 0; }
    bool c_flag() const { return cpsr & FLAG_C; }
    void set_nzcv(bool n, bool z, bool c, bool v) {
        cpsr = (cpsr & ~0xF0000000u) | (n ? FLAG_N : 0) | (z ? FLAG_Z : 0) |
               (c ? FLAG_C : 0) | (v ? FLAG_V : 0);
    }

    uint8_t* bus(uint32_t addr) { return &mem[addr & (kMemSize - 1)]; }
    uint32_t read32(uint32_t addr) {
        const uint32_t m = kMemSize - 1, a = addr & m;
        return mem[a] | (mem[(a + 1) & m] << 8) | (mem[(a + 2) & m] << 16) |
               (mem[(a + 3) & m] << 24);
    }
    void write32(uint32_t addr, uint32_t v) {
        const uint32_t m = kMemSize - 1, a = addr & m;
        mem[a] = v; mem[(a + 1) & m] = v >> 8;
        mem[(a + 2) & m] = v >> 16; mem[(a + 3) & m] = v >> 24;
    }

    //@LABS-BEGIN 1
//@LABS-SOLUTION
    // Sign-extend the 24-bit branch field and scale to bytes.
    int32_t branch_offset(uint32_t instr) const {
        uint32_t imm = instr & 0x00FFFFFF;
        if (imm & 0x00800000) imm |= 0xFF000000u;   // sign extension
        return static_cast<int32_t>(imm << 2);
    }
//@LABS-STUB
    int32_t branch_offset(uint32_t instr) const {
        // TODO(1): sign-extend imm24 and shift left by two.
        (void)instr;
        return 0;
    }
//@LABS-END

    //@LABS-BEGIN 2
//@LABS-SOLUTION
    // B / BL. `pc` is the address of the branch itself; the executing core
    // reads r15 as pc+8, so the target is pc + 8 + offset and BL saves
    // LR = pc + 4 ("LR = pc - 4" in pipeline terms).
    // Returns cycles: 2S+1N when taken, 1S otherwise.
    unsigned exec_branch(uint32_t instr, uint32_t pc, uint32_t& next_pc) {
        const bool link = instr & (1 << 24);
        const uint32_t target = pc + 8 + branch_offset(instr);
        if (link) r[14] = pc + 4;
        next_pc = target;
        return 3;
    }
//@LABS-STUB
    unsigned exec_branch(uint32_t instr, uint32_t pc, uint32_t& next_pc) {
        // TODO(2): compute target = pc + 8 + offset; BL stores pc + 4 in
        // LR; taken branches cost 2S + 1N.
        (void)instr; (void)pc; (void)next_pc;
        return 1;
    }
//@LABS-END

    //@LABS-BEGIN 3
//@LABS-SOLUTION
    // BX Rm: jump to Rm, bit 0 selects Thumb state.
    unsigned exec_bx(uint32_t instr, uint32_t& next_pc) {
        const uint32_t rm_v = r[instr & 0xF];
        thumb = rm_v & 1;
        next_pc = rm_v & ~1u;
        return 3;
    }
//@LABS-STUB
    unsigned exec_bx(uint32_t instr, uint32_t& next_pc) {
        // TODO(3): destination = Rm with bit0 cleared; bit0 sets Thumb state.
        (void)instr; (void)next_pc;
        return 1;
    }
//@LABS-END

    // ---- Data processing (shared with 03_data_processing) ----
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

    struct AddOut { uint32_t value; bool carry; bool overflow; };
    static AddOut add_with_carry(uint32_t a, uint32_t b, bool c_in) {
        const uint64_t sum =
            static_cast<uint64_t>(a) + b + (c_in ? 1u : 0u);
        const uint32_t res = static_cast<uint32_t>(sum);
        return {res, (sum >> 32) != 0,
               ((~(a ^ b) & (a ^ res)) >> 31) != 0};
    }

    uint32_t exec_arith_value(DpOp op, uint32_t a, uint32_t b, bool s,
                              bool& c_out, bool& v_out) {
        switch (op) {
        case kSUB: case kCMP: {
            const auto o = add_with_carry(a, ~b, true);
            c_out = o.carry; v_out = o.overflow; return o.value;
        }
        case kRSB: {
            const auto o = add_with_carry(~a, b, true);
            c_out = o.carry; v_out = o.overflow; return o.value;
        }
        case kADD: case kCMN: {
            const auto o = add_with_carry(a, b, false);   // no carry-in: ADC does that
            c_out = o.carry; v_out = o.overflow; return o.value;
        }
        case kADC: {
            const auto o = add_with_carry(a, b, c_flag());
            c_out = o.carry; v_out = o.overflow; return o.value;
        }
        case kSBC: {
            const auto o = add_with_carry(a, ~b, c_flag());
            c_out = o.carry; v_out = o.overflow; return o.value;
        }
        case kRSC: {
            const auto o = add_with_carry(~a, b, c_flag());
            c_out = o.carry; v_out = o.overflow; return o.value;
        }
        default: c_out = c_flag(); v_out = cpsr & FLAG_V; return 0;
        }
    }

    unsigned exec_dp(uint32_t instr) {
        const DpOp op = static_cast<DpOp>((instr >> 21) & 0xF);
        const bool s = instr & (1 << 20);
        const uint32_t rn = (instr >> 16) & 0xF;
        const uint32_t rd = (instr >> 12) & 0xF;
        const uint32_t op2 = read_operand2(instr);

        if (!op_is_arithmetic(op)) {
            const bool v_old = cpsr & FLAG_V;
            uint32_t res = 0;
            switch (op) {
            case kAND: case kTST: res = r[rn] & op2; break;
            case kEOR: case kTEQ: res = r[rn] ^ op2; break;
            case kORR:            res = r[rn] | op2; break;
            case kMOV:            res = op2;         break;
            case kBIC:            res = r[rn] & ~op2; break;
            case kMVN:            res = ~op2;        break;
            default:              break;
            }
            if (s)
                set_nzcv(flag_n(res), flag_z(res),
                         shifter_carry_valid ? shifter_carry : c_flag(),
                         v_old);
            if (op_writes_result(op)) r[rd] = res;
        } else {
            bool c_alu, v_alu;
            const uint32_t res =
                exec_arith_value(op, r[rn], op2, s, c_alu, v_alu);
            if (s) set_nzcv(flag_n(res), flag_z(res), c_alu, v_alu);
            if (op_writes_result(op)) r[rd] = res;
        }
        return 1;
    }

    // ---- Single load/store (shared with 04_loadstore) ----
    uint32_t transfer_address(uint32_t instr) {
        const uint32_t rn = (instr >> 16) & 0xF;
        const bool u = instr & (1 << 23);
        const bool pre = instr & (1 << 24);
        const bool wb = instr & (1 << 21);
        const int32_t off =
            (instr & (1 << 25))
                ? static_cast<int32_t>(r[instr & 0xF])
                : static_cast<int32_t>(instr & 0xFFF);
        const uint32_t base = r[rn];
        const uint32_t addr = u ? base + static_cast<uint32_t>(off)
                                : base - static_cast<uint32_t>(off);
        if (!pre || wb) r[rn] = addr;
        return pre ? addr : base;
    }

    unsigned exec_ls(uint32_t instr) {
        const bool l = instr & (1 << 20);
        const bool byte = instr & (1 << 22);
        const uint32_t rd = (instr >> 12) & 0xF;
        const uint32_t addr = transfer_address(instr);
        const uint32_t m = kMemSize - 1;
        if (l) {
            if (byte)
                r[rd] = mem[addr & m];
            else {
                const uint32_t aligned = read32(addr & ~3u);
                const uint32_t rot = (addr & 3) * 8;
                r[rd] = (aligned >> rot) |
                        (rot ? aligned << (32 - rot) : 0);
            }
        } else if (byte) {
            mem[addr & m] = static_cast<uint8_t>(r[rd]);
        } else {
            write32(addr, r[rd]);
        }
        return 2;
    }

    //@LABS-BEGIN 4
//@LABS-SOLUTION
    // Fetch, condition-check and execute one ARM instruction.
    // Returns cycles consumed. This explicit step() boundary is what makes
    // the core traceable and testable (curriculum §56).
    unsigned step() {
        const uint32_t pc = r[15];
        const uint32_t instr = read32(pc);
        uint32_t next = pc + 4;                 // pipeline fall-through
        unsigned cycles;

        if (!cond_pass(instr >> 28, cpsr)) {
            cycles = 1;                          // skipped: 1S
        } else if ((instr & 0x0E000000) == 0x0A000000) {
            cycles = exec_branch(instr, pc, next);
        } else if ((instr & 0x0FFFFFF0) == 0x012FFF10) {
            cycles = exec_bx(instr, next);
        } else if ((instr & 0x0C000000) == 0x04000000) {
            cycles = exec_ls(instr);
        } else if ((instr & 0x0C000000) == 0x00000000) {
            cycles = exec_dp(instr);
        } else {
            cycles = 1;                          // unimplemented family: NOP
        }
        r[15] = next;
        return cycles;
    }
//@LABS-STUB
    unsigned step() {
        // TODO(4): fetch at r15, evaluate the condition field, dispatch to
        // the right executor, then commit the next PC.
        return 1;
    }
//@LABS-END
};

}  // namespace arm
