#pragma once
#include <cstdint>
#include <cstdio>
#include "conditions.hpp"
#include "shifter.hpp"
#include "thumb_decoder.hpp"


namespace debugcore {
using arm::FLAG_N;
using arm::FLAG_Z;
using arm::FLAG_C;
using arm::FLAG_V;
using arm::shift_imm;
using arm::cond_pass;
using thumb::kShift;
using thumb::kAddSub;
using thumb::kImmOp;
using thumb::kAlu;
using thumb::kHiReg;
using thumb::kPcRel;
using thumb::kCondBranch;
using thumb::kBranch;
using thumb::kBlFirst;
using thumb::kBlSecond;
using thumb::kF3MOV;
using thumb::kF3CMP;
using thumb::kF3SUB;
using thumb::kF5ADD;
using thumb::kF5CMP;
using thumb::kBX;
using thumb::branch_offset;

// Debuggable Thumb core: same explicit pipeline as 02_pipeline plus a
// minimal SWI/return path so PC/link semantics can be exercised end to end.
// FIVE defects are seeded in the skeleton (each its own @LABS task);
// DEBUGGING.md lists the observed symptoms.
struct DebugCpu {
    static constexpr uint32_t kMemSize = 64 * 1024;
    uint8_t mem[kMemSize] = {};
    uint32_t r[16] = {};
    uint32_t cpsr = 0x00000010;
    bool t = true;

    // Pipeline latches (see 02_pipeline).
    uint16_t fetched_hw = 0;
    uint32_t instr_addr = 0;
    thumb::Decoded decoded{};
    uint32_t bl_high = 0;
    uint32_t spsr_svc = 0;          // saved CPSR across SWI

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

    // Provided: fetch latches the instruction address, then advances PC by
    // ONE THUMB SLOT (two bytes).
    uint16_t fetch() {
        instr_addr = r[15];
        fetched_hw = read16(r[15]);
        r[15] += 2;
        return fetched_hw;
    }

    // Provided: decode into the latch.
    void decode() {
        const uint16_t hw = fetched_hw;
        if (decode_data(hw, decoded)) return;
        if (decode_reg(hw, decoded)) return;
        if (decode_mem(hw, decoded)) return;
        if (decode_branch(hw, decoded)) return;
        decoded.fmt = 0;
    }

    //@LABS-BEGIN 1
//@LABS-SOLUTION
    // Stage 1: FETCH reads the halfword at PC and advances PC by exactly
    // two bytes — the Thumb instruction size. Anything else desyncs the
    // whole stream.
    unsigned stride() { return 2; }
//@LABS-STUB
    // BUG(1): suspicious fetch advance — trace shows every other Thumb
    // halfword being skipped on linear runs.
    unsigned stride() { return 4; }
//@LABS-END

    //@LABS-BEGIN 2
//@LABS-SOLUTION
    // Format 6 literal loads read [instr_addr + 4 + imm*4]: an executing
    // Thumb instruction sees PC = own address + 4.
    uint32_t literal_base() { return instr_addr + 4; }
//@LABS-STUB
    // BUG(2): literal pools read from the wrong slot; values come back
    // shifted by one word whenever imm > 0.
    uint32_t literal_base() { return instr_addr + 8; }
//@LABS-END

    //@LABS-BEGIN 3
//@LABS-SOLUTION
    // B<cond> target: relative to instr_addr + 4 (the PC the branch reads).
    uint32_t branch_target(uint16_t imm8, unsigned width_bits) {
        return instr_addr + 4 +
               static_cast<uint32_t>(branch_offset(imm8, width_bits));
    }
//@LABS-STUB
    // BUG(3): conditional branches land one halfword short of where they
    // should; loops drift backwards until they re-run setup code.
    uint32_t branch_target(uint16_t imm8, unsigned width_bits) {
        return instr_addr + 2 +
               static_cast<uint32_t>(branch_offset(imm8, width_bits));
    }
//@LABS-END

    //@LABS-BEGIN 4
//@LABS-SOLUTION
    // Thumb BL second halfword: offset sext23(bl_high:imm11<<1) relative
    // to first_halfword_addr + 4; LR = that same base.
    unsigned exec_bl_second() {
        uint32_t off = (bl_high << 12) |
                       (static_cast<uint32_t>(decoded.imm) << 1);
        if (bl_high & 0x400) off |= 0xFF800000u;   // sign-extend bit 22
        r[14] = r[15];                             // LR = A + 4
        r[15] += static_cast<int32_t>(off);
        bl_high = 0;
        return 3;
    }
//@LABS-STUB
    // BUG(4): backward BL calls jump into unmapped memory; forward calls
    // land at huge offsets instead of nearby routines.
    unsigned exec_bl_second() {
        uint32_t off = (bl_high << 12) |
                       (static_cast<uint32_t>(decoded.imm) << 1);
        // sign extension "forgotten"
        r[14] = r[15];
        r[15] += static_cast<int32_t>(off);
        bl_high = 0;
        return 3;
    }
//@LABS-END

    // Stage 3: execute the latched instruction (provided; the five buggy
    // helpers above plug in here).
    unsigned execute() {
        const thumb::Decoded d = decoded;
        switch (d.fmt) {
        case kShift: {
            const auto res = shift_imm(d.op, d.imm, r[d.rs], c_flag());
            r[d.rd] = res.value;
            set_nzc(res.value, d.imm ? res.carry_out : c_flag());
            break;
        }
        case kAddSub: {
            const uint32_t b = d.imm_form ? d.rn : r[d.rn];
            const auto o = d.op ? add_with_carry(r[d.rs], ~b, true)
                                : add_with_carry(r[d.rs], b, false);
            r[d.rd] = o.value;
            set_nzcv(flag_n(o.value), flag_z(o.value), o.carry, o.overflow);
            break;
        }
        case kImmOp: {
            switch (d.op) {
            case kF3MOV: r[d.rd] = d.imm; break;
            case kF3CMP: case kF3SUB: {
                const auto o = add_with_carry(r[d.rd], ~d.imm, true);
                set_nzcv(flag_n(o.value), flag_z(o.value), o.carry,
                         o.overflow);
                if (d.op == kF3SUB) r[d.rd] = o.value;
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
        case kHiReg: {
            if (d.op == kBX) {
                const uint32_t target = r[d.rs];
                t = target & 1;
                r[15] = target & ~1u;
                return 3;
            }
            const uint32_t src = r[d.rs];
            switch (d.op) {
            case kF5ADD: r[d.rd] += src; break;
            case kF5CMP: {
                const auto o = add_with_carry(r[d.rd], ~src, true);
                set_nzcv(flag_n(o.value), flag_z(o.value), o.carry,
                         o.overflow);
                return 1;
            }
            default: r[d.rd] = src; break;
            }
            break;
        }
        case kPcRel:
            r[d.rd] = read32(literal_base() + d.imm * 4u);
            break;
        case kCondBranch:
            if (cond_pass(d.op, cpsr)) {
                r[15] = branch_target(d.imm, 8);
                return 3;
            }
            break;
        case kBranch:
            r[15] = branch_target(d.imm, 11);
            return 3;
        case kBlFirst:
            bl_high = d.imm;
            break;
        case kBlSecond:
            return exec_bl_second();
        default:
            break;
        }
        return 1;
    }

    //@LABS-BEGIN 5
//@LABS-SOLUTION
    // Thumb SWI (0xDF imm8): enter supervisor state — save CPSR, vector to
    // the SWI handler at 0x08, LR = next slot. Return undoes it: CPSR must
    // come back from spsr_svc together with the PC.
    unsigned swi() {
        spsr_svc = cpsr;
        r[14] = instr_addr + 4;
        r[15] = 0x08;
        return 3;
    }
    void exception_return() {
        cpsr = spsr_svc;                           // restore flags AND mode
        r[15] = r[14];
    }
//@LABS-STUB
    // BUG(5): after any SWI the flags/mode never recover — the return
    // restores the PC but leaves the supervisor CPSR in place.
    unsigned swi() {
        spsr_svc = cpsr;
        r[14] = instr_addr + 4;
        r[15] = 0x08;
        return 3;
    }
    void exception_return() {
        // TODO(5): restore CPSR from spsr_svc before jumping to LR.
        r[15] = r[14];
    }
//@LABS-END

    // One pipeline pass; SWI dispatch included.
    unsigned step() {
        fetch();
        decode();
        if (fetched_hw >> 8 == 0xDF) return swi();  // 1101 1111 imm8
        return execute();
    }
};

}  // namespace debugcore
