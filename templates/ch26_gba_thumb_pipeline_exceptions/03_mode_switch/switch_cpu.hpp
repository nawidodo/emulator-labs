#pragma once
#include <cstdint>
#include "conditions.hpp"
#include "shifter.hpp"
#include "thumb_decoder.hpp"
using arm::FLAG_N;
using arm::FLAG_Z;
using arm::FLAG_C;
using arm::FLAG_V;

namespace dual {

// Dual-state ARM/Thumb CPU: the mode-switch exercise.
//
// Same explicit pipeline model as 02_pipeline, extended with an ARM state:
//   Thumb step: fetch halfword at PC, PC advances by 2.
//   ARM   step: fetch word at PC,      PC advances by 4.
// Executing instructions read the PC as instr_addr + 4 (Thumb) or
// instr_addr + 8 (ARM) — both computed from `instr_addr`, never from the
// one-slot-advanced r[15].
//
// Mode changes happen ONLY through BX in this model (ARMv4T has no BLX):
// BX Rm sets T = Rm<0> and jumps to Rm & ~1. A safe interleave therefore
// saves the even return address before switching and restores it after.
struct SwitchCpu {
    static constexpr uint32_t kMemSize = 64 * 1024;
    uint8_t mem[kMemSize] = {};
    uint32_t r[16] = {};
    uint32_t cpsr = 0x00000010;
    bool t = false;                       // start in ARM state

    // Pipeline latches (see 02_pipeline for the reasoning).
    uint32_t instr_addr = 0;
    uint32_t fetched_word = 0;            // ARM state
    uint16_t fetched_hw = 0;              // Thumb state
    thumb::Decoded decoded{};
    uint32_t bl_high = 0;

    static bool flag_n(uint32_t v) { return v >> 31; }
    static bool flag_z(uint32_t v) { return v == 0; }
    bool c_flag() const { return cpsr & FLAG_C; }
    void set_nzcv(bool n, bool z, bool c, bool v) {
        cpsr = (cpsr & ~0xF0000000u) | (n ? FLAG_N : 0) | (z ? FLAG_Z : 0) |
               (c ? FLAG_C : 0) | (v ? FLAG_V : 0);
    }
    void set_nzc(uint32_t res, bool c) {
        set_nzcv(flag_n(res), flag_z(res), c, cpsr & FLAG_V);
    }

    struct AddOut { uint32_t value; bool carry; bool overflow; };
    static AddOut add_with_carry(uint32_t a, uint32_t b, bool c_in) {
        const uint64_t sum =
            static_cast<uint64_t>(a) + b + (c_in ? 1u : 0u);
        const uint32_t res = static_cast<uint32_t>(sum);
        return {res, (sum >> 32) != 0, ((~(a ^ b) & (a ^ res)) >> 31) != 0};
    }

    uint16_t read16(uint32_t addr) const {
        const uint32_t m = kMemSize - 1, a = addr & m;
        return static_cast<uint16_t>(mem[a] | (mem[(a + 1) & m] << 8));
    }
    void write16(uint32_t addr, uint16_t v) {
        const uint32_t m = kMemSize - 1, a = addr & m;
        mem[a] = v; mem[(a + 1) & m] = v >> 8;
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

    // ---- Provided: minimal ARM executor subset used by fixtures ----
    // Data processing with immediate or register operand (no shift amounts).
    unsigned exec_arm_dp(uint32_t instr) {
        const uint32_t op = (instr >> 21) & 0xF;
        const bool s = instr & (1 << 20);
        const uint32_t rn = (instr >> 16) & 0xF, rd = (instr >> 12) & 0xF;
        uint32_t op2;
        if (instr & (1 << 25)) {
            const uint32_t rot = ((instr >> 8) & 0xF) * 2 % 32;
            const uint32_t imm8 = instr & 0xFF;
            op2 = rot ? (imm8 >> rot) | (imm8 << (32 - rot)) : imm8;
        } else {
            op2 = r[instr & 0xF];
        }
        uint32_t res = 0;
        switch (op) {
        case 0x2: {  // SUB / CMP
            const auto o = add_with_carry(r[rn], ~op2, true);
            res = o.value;
            if (s) set_nzcv(flag_n(res), flag_z(res), o.carry, o.overflow);
            break;
        }
        case 0x4: {  // ADD / CMN
            const auto o = add_with_carry(r[rn], op2, false);
            res = o.value;
            if (s) set_nzcv(flag_n(res), flag_z(res), o.carry, o.overflow);
            break;
        }
        case 0xD: res = op2; break;                 // MOV
        default: break;
        }
        if (op != 0x2 && op != 0x8 && s && op != 0xA)
            set_nzcv(flag_n(res), flag_z(res), c_flag(), cpsr & FLAG_V);
        if (op != 0x8 && op != 0xA && op != 0xB && rd != 15)
            r[rd] = res;                            // compare family: no write
        return 1;
    }

    // LDR/STR immediate offset, word only.
    unsigned exec_arm_ls(uint32_t instr) {
        const bool load = instr & (1 << 20);
        const uint32_t rn = (instr >> 16) & 0xF, rd = (instr >> 12) & 0xF;
        const uint32_t addr = r[rn] + (instr & 0xFFF);
        if (load) r[rd] = read32(addr);
        else write32(addr, r[rd]);
        return 2;
    }

    //@LABS-BEGIN 1
//@LABS-SOLUTION
    // ARM B / BL. The executing instruction reads PC as instr_addr + 8,
    // so the target is instr_addr + 8 + sext24<<2 and BL links
    // LR = instr_addr + 4 (the slot after the branch). Taken cost 3.
    unsigned exec_arm_branch(uint32_t instr) {
        const bool link = instr & (1 << 24);
        int32_t off = static_cast<int32_t>((instr & 0x00FFFFFF) << 2);
        if (instr & 0x00800000) off |= static_cast<int32_t>(0xFC000000u);
        if (link) r[14] = instr_addr + 4;
        r[15] = instr_addr + 8 + static_cast<uint32_t>(off);
        return 3;
    }
//@LABS-STUB
    unsigned exec_arm_branch(uint32_t instr) {
        // TODO(1): sign-extend imm24<<2, target = instr_addr + 8 + offset;
        // BL stores LR = instr_addr + 4. Taken branches cost 3 cycles.
        (void)instr;
        return 1;
    }
//@LABS-END

    //@LABS-BEGIN 2
//@LABS-SOLUTION
    // BX — THE mode-switch instruction of ARMv4T: jump to target with
    // bit0 cleared and hand bit0 to the T bit. Works identically from
    // either state; costs a refill (3).
    unsigned do_bx(uint32_t target) {
        t = target & 1;
        r[15] = target & ~1u;
        return 3;
    }
//@LABS-STUB
    unsigned do_bx(uint32_t target) {
        // TODO(2): set T from target bit 0, jump to target with bit 0
        // masked off. Taken cost 3 cycles.
        (void)target;
        return 1;
    }
//@LABS-END

    //@LABS-BEGIN 3
//@LABS-SOLUTION
    // Thumb BL second halfword. bl_high holds the first halfword's imm11.
    // LR = first_addr + 4 == second_addr + 2 — exactly what r[15] already
    // holds here (one slot past the second halfword).
    unsigned thumb_bl_second() {
        uint32_t off = (bl_high << 12) | (static_cast<uint32_t>(decoded.imm) << 1);
        if (bl_high & 0x400) off |= 0xFF800000u;
        r[14] = r[15];
        r[15] += static_cast<int32_t>(off);
        bl_high = 0;
        return 3;
    }
//@LABS-STUB
    unsigned thumb_bl_second() {
        // TODO(3): combine bl_high:imm11 into a sign-extended byte offset,
        // link LR = r[15] (first_addr + 4), branch. Taken cost 3.
        return 1;
    }
//@LABS-END

    // ---- Provided: Thumb executor (mirrors 02_pipeline) ----
    unsigned execute_thumb() {
        const thumb::Decoded d = decoded;
        switch (d.fmt) {
        case thumb::kShift: {
            const auto res =
                arm::shift_imm(d.op, d.imm, r[d.rs], c_flag());
            r[d.rd] = res.value;
            set_nzcv(flag_n(res.value), flag_z(res.value),
                     d.imm ? res.carry_out : c_flag(), cpsr & FLAG_V);
            break;
        }
        case thumb::kAddSub: {
            const uint32_t b = d.imm_form ? d.rn : r[d.rn];
            const auto o = d.op ? add_with_carry(r[d.rs], ~b, true)
                                : add_with_carry(r[d.rs], b, false);
            r[d.rd] = o.value;
            set_nzcv(flag_n(o.value), flag_z(o.value), o.carry, o.overflow);
            break;
        }
        case thumb::kImmOp: {
            switch (d.op) {
            case thumb::kF3MOV: r[d.rd] = d.imm; break;
            case thumb::kF3CMP: case thumb::kF3SUB: {
                const auto o = add_with_carry(r[d.rd], ~d.imm, true);
                set_nzcv(flag_n(o.value), flag_z(o.value), o.carry,
                         o.overflow);
                if (d.op == thumb::kF3SUB) r[d.rd] = o.value;
                break;
            }
            default: {
                const auto o = add_with_carry(r[d.rd], d.imm, false);
                r[d.rd] = o.value;
                set_nzcv(flag_n(o.value), flag_z(o.value), o.carry,
                         o.overflow);
                break;
            }
            }
            break;
        }
        case thumb::kHiReg: {
            if (d.op == thumb::kBX) return do_bx(r[d.rs]);
            const uint32_t src = r[d.rs];
            switch (d.op) {
            case thumb::kF5ADD: r[d.rd] += src; break;
            case thumb::kF5CMP: {
                const auto o = add_with_carry(r[d.rd], ~src, true);
                set_nzcv(flag_n(o.value), flag_z(o.value), o.carry,
                         o.overflow);
                return 1;
            }
            default: r[d.rd] = src; break;
            }
            break;
        }
        case thumb::kPcRel:
            r[d.rd] = read32(instr_addr + 4 + d.imm * 4u);
            break;
        case thumb::kCondBranch:
            if (arm::cond_pass(d.op, cpsr)) {
                r[15] = instr_addr + 4 +
                        static_cast<uint32_t>(
                            thumb::branch_offset(d.imm, 8));
                return 3;
            }
            break;
        case thumb::kBranch:
            r[15] = instr_addr + 4 +
                    static_cast<uint32_t>(thumb::branch_offset(d.imm, 11));
            return 3;
        case thumb::kBlFirst:
            bl_high = d.imm;
            break;
        case thumb::kBlSecond:
            return thumb_bl_second();
        default:
            break;
        }
        return 1;
    }

    //@LABS-BEGIN 4
//@LABS-SOLUTION
    // One pipeline pass in EITHER state. Fetch width follows T; decode and
    // execute dispatch on the same latched data.
    unsigned step() {
        instr_addr = r[15];
        unsigned cycles;
        if (t) {
            fetched_hw = read16(instr_addr);
            r[15] += 2;
            decoded = thumb::Decoded{};
            thumb::decode(fetched_hw, decoded);
            cycles = execute_thumb();
        } else {
            fetched_word = read32(instr_addr);
            r[15] += 4;
            cycles = execute_arm();
        }
        return cycles;
    }

    // ARM-side fetch/decode/dispatch.
    unsigned execute_arm() {
        const uint32_t instr = fetched_word;
        if ((instr >> 28) != 0xE && !arm::cond_pass(instr >> 28, cpsr))
            return 1;                               // condition failed: 1S
        if ((instr & 0x0E000000) == 0x0A000000)
            return exec_arm_branch(instr);
        if ((instr & 0x0FFFFFF0) == 0x012FFF10)
            return do_bx(r[instr & 0xF]);
        if ((instr & 0x0C000000) == 0x04000000)
            return exec_arm_ls(instr);
        if ((instr & 0x0C000000) == 0x00000000)
            return exec_arm_dp(instr);
        return 1;                                   // unknown: NOP
    }
//@LABS-STUB
    unsigned step() {
        // TODO(4): latch instr_addr = r[15]; when T, fetch a halfword,
        // advance by 2, decode and run the Thumb executor; otherwise fetch
        // a word, advance by 4, and run execute_arm().
        return 1;
    }
//@LABS-END
};

}  // namespace dual
