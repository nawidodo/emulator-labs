#pragma once
#include <cstdint>
#include <cstdio>
#include "conditions.hpp"
#include "shifter.hpp"
#include "thumb_decoder.hpp"

namespace thumb {
using arm::FLAG_N;
using arm::FLAG_Z;
using arm::FLAG_C;
using arm::FLAG_V;
using arm::shift_imm;
using arm::cond_pass;

// Explicit-pipeline Thumb CPU (fetch -> decode -> execute).
// The pipeline invariant this chapter drills: after fetch(), r[15] already
// points at instr_addr + 4, and every PC-relative computation during
// execute MUST use that advanced value (Thumb prefetch distance).
struct ThumbCpu {
    static constexpr uint32_t kMemSize = 64 * 1024;
    uint8_t mem[kMemSize] = {};
    uint32_t r[16] = {};
    uint32_t cpsr = 0x00000010;
    bool t = true;

    // Pipeline latches: one decoded instruction between fetch and execute.
    // `instr_addr` remembers WHERE the fetched halfword lives; r[15] runs
    // one slot ahead of it (single prefetch in this model), so the
    // architectural "PC = instr_addr + 4" an instruction must read is
    // computed from the latch, never from r[15] directly.
    uint16_t fetched_hw = 0;
    uint32_t instr_addr = 0;
    Decoded decoded{};
    uint32_t bl_high = 0;   // first BL halfword scratch

    static bool flag_n(uint32_t v) { return v >> 31; }
    static bool flag_z(uint32_t v) { return v == 0; }
    bool c_flag() const { return cpsr & FLAG_C; }
    void set_nzcv(bool n, bool z, bool c, bool v) {
        cpsr = (cpsr & ~0xF0000000u) | (n ? FLAG_N : 0) | (z ? FLAG_Z : 0) |
               (c ? FLAG_C : 0) | (v ? FLAG_V : 0);
    }
    void set_nzc(uint32_t res, bool c) { set_nzcv(flag_n(res), flag_z(res), c, cpsr & FLAG_V); }

    struct AddOut { uint32_t value; bool carry; bool overflow; };
    static AddOut add_with_carry(uint32_t a, uint32_t b, bool c_in) {
        const uint64_t sum = static_cast<uint64_t>(a) + b + (c_in ? 1u : 0u);
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

    //@LABS-BEGIN 1
//@LABS-SOLUTION
    // Stage 1: FETCH. Read the halfword at PC, remember its address in
    // `instr_addr`, then advance PC by one slot.
    uint16_t fetch() {
        instr_addr = r[15];
        fetched_hw = read16(r[15]);
        r[15] += 2;
        return fetched_hw;
    }
//@LABS-STUB
    uint16_t fetch() {
        // TODO(1): read the halfword at PC into fetched_hw, latch its
        // address in instr_addr, and advance PC by two.
        return 0;
    }
//@LABS-END

    //@LABS-BEGIN 2
//@LABS-SOLUTION
    // Stage 2: DECODE into the pipeline latch.
    void decode() {
        const uint16_t hw = fetched_hw;
        if (decode_data(hw, decoded)) return;
        if (decode_reg(hw, decoded)) return;
        if (decode_mem(hw, decoded)) return;
        if (decode_branch(hw, decoded)) return;
        decoded.fmt = 0;                       // unknown: treated as NOP
    }
//@LABS-STUB
    void decode() {
        // TODO(2): run fetched_hw through the four decoder helpers and
        // latch the result in `decoded` (unknown halfwords latch fmt=0).
    }
//@LABS-END

    //@LABS-BEGIN 3
//@LABS-SOLUTION
    // Stage 3: EXECUTE the latched instruction.
    // Returns cycles consumed (taken branches pay 2S+1N refill = 3).
    unsigned execute() {
        const Decoded d = decoded;
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
            default: {  // ADD
                const auto o = add_with_carry(r[d.rd], d.imm, false);
                r[d.rd] = o.value;
                set_nzcv(flag_n(o.value), flag_z(o.value), o.carry,
                         o.overflow);
                break;
            }
            }
            break;
        }
        case kAlu: {
            const uint32_t a = r[d.rd], b = r[d.rs];
            uint32_t res = 0;
            bool c_out = c_flag();
            switch (d.op) {
            case 0: res = a & b; break;                 // AND
            case 1: res = a ^ b; break;                 // EOR
            case 5: case 6: {                           // ADC / SBC
                const auto o = d.op == 5 ? add_with_carry(a, b, c_flag())
                                         : add_with_carry(a, ~b, c_flag());
                res = o.value; c_out = o.carry;
                set_nzcv(flag_n(res), flag_z(res), o.carry, o.overflow);
                r[d.rd] = res;
                return 1;
            }
            case 8: res = a & b; set_nzcv(flag_n(res), flag_z(res),
                                          c_flag(), cpsr & FLAG_V);
                    return 1;                           // TST
            case 10: {                                  // CMP
                const auto o = add_with_carry(a, ~b, true);
                set_nzcv(flag_n(o.value), flag_z(o.value), o.carry,
                         o.overflow);
                return 1;
            }
            case 11: {                                  // CMN
                const auto o = add_with_carry(a, b, false);
                set_nzcv(flag_n(o.value), flag_z(o.value), o.carry,
                         o.overflow);
                return 1;
            }
            case 12: res = a | b; break;                // ORR
            case 14: res = a & ~b; break;               // BIC
            case 15: res = ~b; break;                   // MVN
            default: res = b; break;
            }
            r[d.rd] = res;
            set_nzc(res, c_out);
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
            case kF5ADD: r[d.rd] = r[d.rd] + src; break;
            case kF5CMP: {
                const auto o = add_with_carry(r[d.rd], ~src, true);
                set_nzcv(flag_n(o.value), flag_z(o.value), o.carry,
                         o.overflow);
                return 1;
            }
            default: r[d.rd] = src; break;              // MOV
            }
            break;
        }
        case kPcRel: {
            // THE pipeline lesson: an executing Thumb instruction reads
            // PC as its own address + 4 (two prefetched slots). r[15] in
            // this model only runs one slot ahead, so use the latch.
            r[d.rd] = read32(instr_addr + 4 + d.imm * 4u);
            break;
        }
        case kCondBranch: {
            if (cond_pass(d.op, cpsr)) {
                // Targets are relative to instr_addr + 4, not to the
                // single-slot-advanced r[15].
                r[15] = instr_addr + 4 +
                        static_cast<uint32_t>(branch_offset(d.imm, 8));
                return 3;
            }
            break;
        }
        case kBranch: {
            r[15] = instr_addr + 4 +
                    static_cast<uint32_t>(branch_offset(d.imm, 11));
            return 3;
        }
        case kBlFirst: {
            bl_high = d.imm;
            break;
        }
        case kBlSecond: {
            // Offset = sext23(high11 : low11 : 0) relative to
            // first_halfword_addr + 4. At this point instr_addr is the
            // SECOND halfword's address and r[15] == instr_addr + 2, i.e.
            // both equal first_addr + 4 — the architectural link value.
            uint32_t off = (bl_high << 12) | (d.imm << 1);
            if (bl_high & 0x400) off |= 0xFF800000u;
            r[14] = r[15];                              // LR = A + 4
            r[15] += static_cast<int32_t>(off);
            bl_high = 0;
            return 3;
        }
        default:
            break;                                      // NOP / unknown
        }
        return 1;
    }
//@LABS-STUB
    unsigned execute() {
        // TODO(3): dispatch on decoded.fmt and perform each operation.
        // Remember: PC-relative accesses and branch targets both start
        // from the ALREADY ADVANCED r[15]; taken branches cost 3 cycles.
        return 1;
    }
//@LABS-END

    //@LABS-BEGIN 4
//@LABS-SOLUTION
    // One full pass through the pipeline.
    unsigned step() {
        fetch();
        decode();
        return execute();
    }
//@LABS-STUB
    unsigned step() {
        // TODO(4): wire fetch -> decode -> execute together.
        return 1;
    }
//@LABS-END
};

}  // namespace thumb
