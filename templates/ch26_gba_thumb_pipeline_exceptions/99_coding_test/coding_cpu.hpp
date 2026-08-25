#pragma once
#include <cstdint>
#include <cstdio>
#include "conditions.hpp"
#include "shifter.hpp"
#include "thumb_decoder.hpp"

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

namespace coding {

// CODING TEST core: an explicit-pipeline Thumb CPU whose control flow,
// literal, compare and cycle-accounting behavior is seeded with FIVE
// defects in the skeleton. The hidden grader runs fixture programs through
// the chapter runner and hashes the exact traces — every wrong PC, wrong
// register or off-by-one cycle count is visible in the golden comparison.
// Fix each helper below; DEBUGGING-style symptoms are listed in
// CODING_TEST.md.
struct CodingCpu {
    static constexpr uint32_t kMemSize = 64 * 1024;
    uint8_t mem[kMemSize] = {};
    uint32_t r[16] = {};
    uint32_t cpsr = 0x00000010;
    bool t = true;

    uint16_t fetched_hw = 0;
    uint32_t instr_addr = 0;
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

    // Provided: fetch latches the address and advances one Thumb slot.
    uint16_t fetch() {
        instr_addr = r[15];
        fetched_hw = read16(r[15]);
        r[15] += 2;
        return fetched_hw;
    }
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
    // Unconditional B target: sext11(imm)*2 relative to instr_addr + 4.
    uint32_t uncond_target(uint16_t imm11) {
        return instr_addr + 4 +
               static_cast<uint32_t>(thumb::branch_offset(imm11, 11));
    }
//@LABS-STUB
    // BUG(1): backward unconditional branches escape into high memory.
    uint32_t uncond_target(uint16_t imm11) {
        return instr_addr + 4 + static_cast<uint32_t>(imm11 << 1);
    }
//@LABS-END

    //@LABS-BEGIN 2
//@LABS-SOLUTION
    // Format-3 CMP: flags only, Rd untouched.
    void cmp_imm8(uint32_t rd, uint16_t imm8) {
        const auto o = add_with_carry(r[rd], ~imm8, true);
        set_nzcv(flag_n(o.value), flag_z(o.value), o.carry, o.overflow);
    }
//@LABS-STUB
    // BUG(2): after any CMP the compared register holds the difference.
    void cmp_imm8(uint32_t rd, uint16_t imm8) {
        const auto o = add_with_carry(r[rd], ~imm8, true);
        set_nzcv(flag_n(o.value), flag_z(o.value), o.carry, o.overflow);
        r[rd] = o.value;
    }
//@LABS-END

    //@LABS-BEGIN 3
//@LABS-SOLUTION
    // Literal pool address: PC reads as instr_addr + 4; imm is a WORD
    // offset (x4).
    uint32_t literal_addr(uint16_t imm8) {
        return instr_addr + 4 + static_cast<uint32_t>(imm8) * 4u;
    }
//@LABS-STUB
    // BUG(3): literal loads read halfword-scaled offsets — wrong word for
    // any imm > 0.
    uint32_t literal_addr(uint16_t imm8) {
        return instr_addr + 4 + static_cast<uint32_t>(imm8) * 2u;
    }
//@LABS-END

    //@LABS-BEGIN 4
//@LABS-SOLUTION
    // Cycle accounting: a taken branch flushes decode+fetch: 2S+1N -> 3.
    unsigned taken_cycles() { return 3; }
//@LABS-STUB
    // BUG(4): every trace's cumulative cyc column drifts low across any
    // taken branch.
    unsigned taken_cycles() { return 2; }
//@LABS-END

    //@LABS-BEGIN 5
//@LABS-SOLUTION
    // Thumb BL second halfword: LR must point at the halfword AFTER the
    // pair (first_addr + 4), which equals this step's advanced r[15].
    unsigned exec_bl_second() {
        uint32_t off = (bl_high << 12) |
                       (static_cast<uint32_t>(decoded.imm) << 1);
        if (bl_high & 0x400) off |= 0xFF800000u;
        r[14] = r[15];
        r[15] += static_cast<int32_t>(off);
        bl_high = 0;
        return taken_cycles();
    }
//@LABS-STUB
    // BUG(5): BL returns land one halfword early — re-running the second
    // halfword as code.
    unsigned exec_bl_second() {
        uint32_t off = (bl_high << 12) |
                       (static_cast<uint32_t>(decoded.imm) << 1);
        if (bl_high & 0x400) off |= 0xFF800000u;
        r[14] = instr_addr;                    // one slot short of A + 4
        r[15] += static_cast<int32_t>(off);
        bl_high = 0;
        return taken_cycles();
    }
//@LABS-END

    // Provided: executor dispatching through the five helpers above.
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
            case kF3CMP:
                cmp_imm8(d.rd, d.imm);
                break;
            case kF3SUB: {
                const auto o = add_with_carry(r[d.rd], ~d.imm, true);
                set_nzcv(flag_n(o.value), flag_z(o.value), o.carry,
                         o.overflow);
                r[d.rd] = o.value;
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
        case kAlu: {
            const uint32_t a = r[d.rd], b = r[d.rs];
            uint32_t res = 0;
            switch (d.op) {
            case 0: res = a & b; break;                 // AND
            case 1: res = a ^ b; break;                 // EOR
            case 10: res = a - b; break;                // CMP reg (flags only)
            case 12: res = a | b; break;                // ORR
            case 15: res = ~b; break;                   // MVN
            default: res = b; break;
            }
            if (d.op != 10) r[d.rd] = res;
            else set_nzc(a - b, a >= b);
            break;
        }
        case kHiReg: {
            if (d.op == kBX) {
                const uint32_t target = r[d.rs];
                t = target & 1;
                r[15] = target & ~1u;
                return taken_cycles();
            }
            const uint32_t src = r[d.rs];
            switch (d.op) {
            case kF5ADD: r[d.rd] += src; break;
            case kF5CMP: cmp_imm8_high(r[d.rd], src); return 1;
            default: r[d.rd] = src; break;
            }
            break;
        }
        case kPcRel:
            r[d.rd] = read32(literal_addr(d.imm));
            break;
        case kCondBranch:
            if (cond_pass(d.op, cpsr)) {
                r[15] = instr_addr + 4 +
                        static_cast<uint32_t>(
                            thumb::branch_offset(d.imm, 8));
                return taken_cycles();
            }
            break;
        case kBranch:
            r[15] = uncond_target(d.imm);
            return taken_cycles();
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

    void cmp_imm8_high(uint32_t a, uint32_t b) {
        const auto o = add_with_carry(a, ~b, true);
        set_nzcv(flag_n(o.value), flag_z(o.value), o.carry, o.overflow);
    }

    unsigned step() {
        fetch();
        decode();
        return execute();
    }
};

}  // namespace coding
