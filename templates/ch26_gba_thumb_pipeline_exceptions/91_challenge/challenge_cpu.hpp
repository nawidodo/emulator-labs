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
using thumb::kBX;
using thumb::kPush;
using thumb::kPop;
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

namespace challenge {

// Challenge CPU: the 02_pipeline Thumb core extended with the stack
// operations PUSH/POP (formats already decoded as kPush/kPop). The stack
// is FULL DESCENDING: SP points at the last used word, PUSH pre-decrements,
// POP post-increments. Lowest register always maps to the lowest address.
struct ChallengeCpu {
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
    // PUSH {reglist[, LR]}: full-descending store. Start at SP - 4*count,
    // store lowest register at the lowest address, writeback SP.
    // Modeled cost: nS + 1N (one extra cycle for the stack address).
    unsigned exec_push(uint32_t list) {
        unsigned n = 0;
        for (uint32_t i = 0; i < 9; ++i) n += (list >> i) & 1;
        uint32_t addr = r[13] - 4 * n;
        for (uint32_t rg = 0; rg < 8; ++rg) {
            if (!(list & (1u << rg))) continue;
            write32(addr, r[rg]);
            addr += 4;
        }
        if (list & (1u << 8)) write32(addr, r[14]);   // LR stored last
        r[13] -= 4 * n;
        return n + 1;
    }
//@LABS-STUB
    unsigned exec_push(uint32_t list) {
        // TODO(1): full-descending PUSH — start at SP - 4*count, store
        // registers lowest-at-lowest-address (bit8 = LR goes last), then
        // write back SP. Returns n+1 cycles.
        (void)list;
        return 1;
    }
//@LABS-END

    //@LABS-BEGIN 2
//@LABS-SOLUTION
    // POP {reglist[, PC]}: load ascending addresses into ascending
    // registers, post-increment SP. Loading PC branches: refill cost
    // (T state is unchanged on ARMv4T — bit0 of the loaded value is
    // ignored); otherwise plain n+1 cost like PUSH.
    unsigned exec_pop(uint32_t list) {
        uint32_t addr = r[13];
        for (uint32_t rg = 0; rg < 8; ++rg) {
            if (!(list & (1u << rg))) continue;
            r[rg] = read32(addr);
            addr += 4;
        }
        const bool pops_pc = list & (1u << 8);
        if (pops_pc) {
            r[15] = read32(addr) & ~1u;     // T unchanged on ARMv4T
            addr += 4;
        }
        r[13] = addr;
        return pops_pc ? 4 : 3;             // branch refill vs plain pop
    }
//@LABS-STUB
    unsigned exec_pop(uint32_t list) {
        // TODO(2): load ascending words into ascending registers from SP,
        // post-increment SP past every transferred word (bit8 = PC: load,
        // clear bit0 of the value, keep T, pay a branch refill).
        (void)list;
        return 1;
    }
//@LABS-END

    // Provided: executor (identical to 02_pipeline plus PUSH/POP dispatch).
    unsigned execute() {
        const thumb::Decoded d = decoded;
        switch (d.fmt) {
        case kPush: return exec_push(d.imm);
        case kPop: return exec_pop(d.imm);
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
            default: res = b; break;                    // MOV reg form
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
            case kF5ADD: r[d.rd] += src; break;
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
        case kPcRel:
            r[d.rd] = read32(instr_addr + 4 + d.imm * 4u);
            break;
        case kCondBranch:
            if (cond_pass(d.op, cpsr)) {
                r[15] = instr_addr + 4 +
                        static_cast<uint32_t>(branch_offset(d.imm, 8));
                return 3;
            }
            break;
        case kBranch:
            r[15] = instr_addr + 4 +
                    static_cast<uint32_t>(branch_offset(d.imm, 11));
            return 3;
        case kBlFirst:
            bl_high = d.imm;
            break;
        case kBlSecond: {
            uint32_t off = (bl_high << 12) |
                           (static_cast<uint32_t>(d.imm) << 1);
            if (bl_high & 0x400) off |= 0xFF800000u;
            r[14] = r[15];                              // LR = A + 4
            r[15] += static_cast<int32_t>(off);
            bl_high = 0;
            return 3;
        }
        default:
            break;
        }
        return 1;
    }

    unsigned step() {
        fetch();
        decode();
        return execute();
    }
};

}  // namespace challenge
