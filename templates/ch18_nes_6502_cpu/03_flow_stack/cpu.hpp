// nes6502.hpp — 6502 core, flow-control exercise.
//
// Loads/stores/ALU/table dispatch already work (exercise 02). This file adds
// compares, INC/DEC, shifts, branches, jumps/subroutines and flag ops —
// again as free functions wired into the same decode table.
#pragma once

#include <array>
#include <cstdint>

namespace nes6502 {

enum FlagBits : uint8_t {
    FC = 0x01, FZ = 0x02, FI = 0x04, FD = 0x08,
    FB = 0x10, FU = 0x20, FV = 0x40, FN = 0x80,
};

class Bus {
public:
    virtual ~Bus() = default;
    virtual uint8_t read(uint16_t addr) = 0;
    virtual void write(uint16_t addr, uint8_t value) = 0;
};

struct FlatRam final : Bus {
    std::array<uint8_t, 0x10000> mem{};
    uint8_t read(uint16_t addr) override { return mem[addr]; }
    void write(uint16_t addr, uint8_t value) override { mem[addr] = value; }
};

struct Cpu {
    uint8_t a = 0, x = 0, y = 0;
    uint8_t sp = 0xFD;
    uint8_t p = FU | FI;
    uint16_t pc = 0;
    uint64_t cycles = 0;
    Bus* bus = nullptr;
    bool halted = false;

    uint8_t read(uint16_t addr) { ++cycles; return bus->read(addr); }
    void write(uint16_t addr, uint8_t v) { ++cycles; bus->write(addr, v); }
    uint8_t fetch8() { return read(pc++); }
    uint16_t fetch16() {
        const uint16_t lo = fetch8();
        return lo | uint16_t(fetch8()) << 8;
    }
    void push(uint8_t v) { write(uint16_t(0x100) | sp, v); --sp; }
    uint8_t pop() { ++sp; return read(uint16_t(0x100) | sp); }

    void set_zn(uint8_t v) {
        p = uint8_t((p & ~(FZ | FN)) | (v == 0 ? FZ : 0) | (v & FN));
    }

    void load_program(uint16_t base, const uint8_t* bytes, size_t n) {
        for (size_t i = 0; i < n; ++i) bus->write(uint16_t(base + i), bytes[i]);
        pc = base;
        cycles = 0;
        halted = false;
    }
};

// ---- addressing modes (complete, from exercises 01/02) --------------------

using ModeFn = bool (*)(Cpu&, uint16_t&);

inline bool mode_imp(Cpu&, uint16_t&) { return false; }
inline bool mode_acc(Cpu&, uint16_t&) { return false; }
inline bool mode_imm(Cpu& c, uint16_t& out) { out = c.pc++; return false; }
inline bool mode_zp(Cpu& c, uint16_t& out) { out = c.fetch8(); return false; }
inline bool mode_zpx(Cpu& c, uint16_t& out) {
    out = uint8_t(c.fetch8() + c.x);
    return false;
}
inline bool mode_zpy(Cpu& c, uint16_t& out) {
    out = uint8_t(c.fetch8() + c.y);
    return false;
}
inline bool mode_abs(Cpu& c, uint16_t& out) { out = c.fetch16(); return false; }
inline bool mode_absx(Cpu& c, uint16_t& out) {
    const uint16_t base = c.fetch16();
    out = uint16_t(base + c.x);
    return (base & 0xFF00) != (out & 0xFF00);
}
inline bool mode_absy(Cpu& c, uint16_t& out) {
    const uint16_t base = c.fetch16();
    out = uint16_t(base + c.y);
    return (base & 0xFF00) != (out & 0xFF00);
}
inline bool mode_izx(Cpu& c, uint16_t& out) {
    const uint8_t ptr = uint8_t(c.fetch8() + c.x);
    out = c.read(ptr) | uint16_t(c.read(uint8_t(ptr + 1))) << 8;
    return false;
}
inline bool mode_izy(Cpu& c, uint16_t& out) {
    const uint8_t ptr = c.fetch8();
    const uint16_t base =
        c.read(ptr) | uint16_t(c.read(uint8_t(ptr + 1))) << 8;
    out = uint16_t(base + c.y);
    return (base & 0xFF00) != (out & 0xFF00);
}
inline bool mode_ind(Cpu& c, uint16_t& out) {
    // JMP ($xxxx): high pointer byte never leaves the pointer's page.
    const uint16_t ptr = c.fetch16();
    const uint16_t lo = c.read(ptr);
    const uint16_t hi =
        c.read(uint16_t((ptr & 0xFF00) | ((ptr + 1) & 0x00FF)));
    out = lo | hi << 8;
    return false;
}

// ---- loads/stores/transfers/logic/arith (complete, from exercise 02) ------

using OpFn = void (*)(Cpu&, uint16_t);

inline void op_lda(Cpu& c, uint16_t ad) { c.a = c.read(ad); c.set_zn(c.a); }
inline void op_ldx(Cpu& c, uint16_t ad) { c.x = c.read(ad); c.set_zn(c.x); }
inline void op_ldy(Cpu& c, uint16_t ad) { c.y = c.read(ad); c.set_zn(c.y); }
inline void op_sta(Cpu& c, uint16_t ad) { c.write(ad, c.a); }
inline void op_stx(Cpu& c, uint16_t ad) { c.write(ad, c.x); }
inline void op_sty(Cpu& c, uint16_t ad) { c.write(ad, c.y); }
inline void op_tax(Cpu& c, uint16_t) {
    ++c.cycles;  // implied ops dummy-read the next byte
    c.x = c.a; c.set_zn(c.x);
}
inline void op_tay(Cpu& c, uint16_t) {
    ++c.cycles;  // implied ops dummy-read the next byte
    c.y = c.a; c.set_zn(c.y);
}
inline void op_txa(Cpu& c, uint16_t) {
    ++c.cycles;  // implied ops dummy-read the next byte
    c.a = c.x; c.set_zn(c.a);
}
inline void op_tya(Cpu& c, uint16_t) {
    ++c.cycles;  // implied ops dummy-read the next byte
    c.a = c.y; c.set_zn(c.a);
}
inline void op_tsx(Cpu& c, uint16_t) {
    ++c.cycles;  // implied ops dummy-read the next byte
    c.x = c.sp; c.set_zn(c.x);
}
inline void op_txs(Cpu& c, uint16_t) {
    ++c.cycles;  // implied ops dummy-read the next byte
    c.sp = c.x;
}
inline void op_pha(Cpu& c, uint16_t) {
    ++c.cycles;  // internal: SP dummy decrement
    c.push(c.a);
}
inline void op_php(Cpu& c, uint16_t) {
    ++c.cycles;  // internal: SP dummy decrement
    c.push(uint8_t(c.p | FB | FU));
}
inline void op_pla(Cpu& c, uint16_t) {
    c.cycles += 2;  // internal: two dummy SP increments
    c.a = c.pop();
    c.set_zn(c.a);
}
inline void op_plp(Cpu& c, uint16_t) {
    c.cycles += 2;  // internal: two dummy SP increments
    c.p = uint8_t((c.pop() & ~FB) | FU);
}
inline void op_and(Cpu& c, uint16_t ad) { c.a &= c.read(ad); c.set_zn(c.a); }
inline void op_ora(Cpu& c, uint16_t ad) { c.a |= c.read(ad); c.set_zn(c.a); }
inline void op_eor(Cpu& c, uint16_t ad) { c.a ^= c.read(ad); c.set_zn(c.a); }
inline void op_bit(Cpu& c, uint16_t ad) {
    const uint8_t m = c.read(ad);
    c.p = uint8_t((c.p & ~(FN | FV | FZ)) | (m & FN) |
                  ((m & FV) ? FV : 0) | ((c.a & m) == 0 ? FZ : 0));
}
inline void adc_raw(Cpu& c, uint8_t m) {
    const unsigned sum = unsigned(c.a) + m + ((c.p & FC) ? 1 : 0);
    c.p = uint8_t(c.p & ~(FC | FV));
    if (sum > 0xFF) c.p |= FC;
    if (~(c.a ^ m) & (c.a ^ sum) & 0x80) c.p |= FV;
    c.a = uint8_t(sum);
    c.set_zn(c.a);
}
inline void op_adc(Cpu& c, uint16_t ad) { adc_raw(c, c.read(ad)); }
inline void op_sbc(Cpu& c, uint16_t ad) { adc_raw(c, uint8_t(~c.read(ad))); }

// ---------------------------------------------------------------------------
// This exercise's work.
// ---------------------------------------------------------------------------

//@LABS-BEGIN 1
//@LABS-SOLUTION
// CMP is a subtraction whose only survivors are C (reg >= m), Z and N.
inline void cmp_reg(Cpu& c, uint8_t reg, uint8_t m) {
    c.p = uint8_t((c.p & ~FC) | (reg >= m ? FC : 0));
    c.set_zn(uint8_t(reg - m));
}

inline void op_cmp(Cpu& c, uint16_t ad) { cmp_reg(c, c.a, c.read(ad)); }
inline void op_cpx(Cpu& c, uint16_t ad) { cmp_reg(c, c.x, c.read(ad)); }
inline void op_cpy(Cpu& c, uint16_t ad) { cmp_reg(c, c.y, c.read(ad)); }
//@LABS-STUB
// TODO(1): compares. Carry is set when the register >= memory (no borrow),
// and Z/N come from the discarded difference.
inline void op_cmp(Cpu&, uint16_t) {}  // TODO(1)
inline void op_cpx(Cpu&, uint16_t) {}  // TODO(1)
inline void op_cpy(Cpu&, uint16_t) {}  // TODO(1)
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Read-modify-write helper: the real 6502 writes the OLD value back before
// the new one (a dummy write other devices can observe). Billing each
// access gives the official 5-cycle zp / 6-cycle abs RMW totals.
using RmwFn = uint8_t (*)(Cpu&, uint8_t);
inline uint8_t rmw(Cpu& c, uint16_t ad, RmwFn f) {
    const uint8_t old = c.read(ad);
    c.write(ad, old);  // dummy write of the original value
    const uint8_t updated = f(c, old);
    c.write(ad, updated);
    return updated;
}

inline uint8_t raw_inc(Cpu&, uint8_t v) { return uint8_t(v + 1); }
inline uint8_t raw_dec(Cpu&, uint8_t v) { return uint8_t(v - 1); }

inline void op_inc(Cpu& c, uint16_t ad) { c.set_zn(rmw(c, ad, raw_inc)); }
inline void op_dec(Cpu& c, uint16_t ad) { c.set_zn(rmw(c, ad, raw_dec)); }
inline void op_inx(Cpu& c, uint16_t) {
    ++c.cycles;  // implied ops dummy-read the next byte
    ++c.x; c.set_zn(c.x);
}
inline void op_iny(Cpu& c, uint16_t) {
    ++c.cycles;  // implied ops dummy-read the next byte
    ++c.y; c.set_zn(c.y);
}
inline void op_dex(Cpu& c, uint16_t) {
    ++c.cycles;  // implied ops dummy-read the next byte
    --c.x; c.set_zn(c.x);
}
inline void op_dey(Cpu& c, uint16_t) {
    ++c.cycles;  // implied ops dummy-read the next byte
    --c.y; c.set_zn(c.y);
}
//@LABS-STUB
// TODO(2): INC/DEC go through rmw() (which models the dummy write) and set
// N/Z from the stored result; INX/INY/DEX/DEY update their register.
inline void op_inc(Cpu&, uint16_t) {}  // TODO(2)
inline void op_dec(Cpu&, uint16_t) {}  // TODO(2)
inline void op_inx(Cpu&, uint16_t) {}  // TODO(2)
inline void op_iny(Cpu&, uint16_t) {}  // TODO(2)
inline void op_dex(Cpu&, uint16_t) {}  // TODO(2)
inline void op_dey(Cpu&, uint16_t) {}  // TODO(2)
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
inline uint8_t raw_asl(Cpu& c, uint8_t v) {
    c.p = uint8_t((c.p & ~FC) | (v >> 7));
    return uint8_t(v << 1);
}
inline uint8_t raw_lsr(Cpu& c, uint8_t v) {
    c.p = uint8_t((c.p & ~FC) | (v & 1));
    return v >> 1;
}
inline uint8_t raw_rol(Cpu& c, uint8_t v) {
    const uint8_t carry_out = uint8_t(v >> 7);
    v = uint8_t((v << 1) | ((c.p & FC) ? 1 : 0));
    c.p = uint8_t((c.p & ~FC) | carry_out);
    return v;
}
inline uint8_t raw_ror(Cpu& c, uint8_t v) {
    const uint8_t carry_out = uint8_t(v & 1);
    v = uint8_t((v >> 1) | ((c.p & FC) ? 0x80 : 0));
    c.p = uint8_t((c.p & ~FC) | carry_out);
    return v;
}

inline void op_asl_a(Cpu& c, uint16_t) {
    ++c.cycles;  // implied ops dummy-read the next byte
    c.a = raw_asl(c, c.a);
    c.set_zn(c.a);
}
inline void op_lsr_a(Cpu& c, uint16_t) {
    ++c.cycles;  // implied ops dummy-read the next byte
    c.a = raw_lsr(c, c.a);
    c.set_zn(c.a);
}
inline void op_rol_a(Cpu& c, uint16_t) {
    ++c.cycles;  // implied ops dummy-read the next byte
    c.a = raw_rol(c, c.a);
    c.set_zn(c.a);
}
inline void op_ror_a(Cpu& c, uint16_t) {
    ++c.cycles;  // implied ops dummy-read the next byte
    c.a = raw_ror(c, c.a);
    c.set_zn(c.a);
}

inline void op_asl(Cpu& c, uint16_t ad) { c.set_zn(rmw(c, ad, raw_asl)); }
inline void op_lsr(Cpu& c, uint16_t ad) { c.set_zn(rmw(c, ad, raw_lsr)); }
inline void op_rol(Cpu& c, uint16_t ad) { c.set_zn(rmw(c, ad, raw_rol)); }
inline void op_ror(Cpu& c, uint16_t ad) { c.set_zn(rmw(c, ad, raw_ror)); }
//@LABS-STUB
// TODO(3): shifts rotate through the carry flag. The *_a forms operate on
// A directly; memory forms go through rmw(). Set N/Z from the final value.
inline void op_asl_a(Cpu&, uint16_t) {}  // TODO(3)
inline void op_lsr_a(Cpu&, uint16_t) {}  // TODO(3)
inline void op_rol_a(Cpu&, uint16_t) {}  // TODO(3)
inline void op_ror_a(Cpu&, uint16_t) {}  // TODO(3)
inline void op_asl(Cpu&, uint16_t) {}    // TODO(3)
inline void op_lsr(Cpu&, uint16_t) {}    // TODO(3)
inline void op_rol(Cpu&, uint16_t) {}    // TODO(3)
inline void op_ror(Cpu&, uint16_t) {}    // TODO(3)
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// Branches cost 2 cycles base; a taken branch bills +1, and +1 more when
// the new PC lands on a different page than the instruction AFTER the
// branch operand (the dummy re-fetch of the second opcode byte).
inline void branch_rel(Cpu& c, bool cond) {
    const auto off = int8_t(c.fetch8());
    if (!cond) return;
    ++c.cycles;
    const uint16_t target = uint16_t(c.pc + off);
    if ((target & 0xFF00) != (c.pc & 0xFF00)) ++c.cycles;
    c.pc = target;
}

inline void op_bpl(Cpu& c, uint16_t) { branch_rel(c, !(c.p & FN)); }
inline void op_bmi(Cpu& c, uint16_t) { branch_rel(c, (c.p & FN) != 0); }
inline void op_bvc(Cpu& c, uint16_t) { branch_rel(c, !(c.p & FV)); }
inline void op_bvs(Cpu& c, uint16_t) { branch_rel(c, (c.p & FV) != 0); }
inline void op_bcc(Cpu& c, uint16_t) { branch_rel(c, !(c.p & FC)); }
inline void op_bcs(Cpu& c, uint16_t) { branch_rel(c, (c.p & FC) != 0); }
inline void op_bne(Cpu& c, uint16_t) { branch_rel(c, !(c.p & FZ)); }
inline void op_beq(Cpu& c, uint16_t) { branch_rel(c, (c.p & FZ) != 0); }
//@LABS-STUB
// TODO(4): conditional branches share branch_rel(): fetch the signed
// offset; if taken bill +1 cycle, +1 more on a page-boundary cross, then
// jump. Flag sense: BPL/BMI test N, BVC/BVS test V, BCC/BCS test C,
// BNE/BEQ test Z.
inline void op_bpl(Cpu&, uint16_t) {}  // TODO(4)
inline void op_bmi(Cpu&, uint16_t) {}  // TODO(4)
inline void op_bvc(Cpu&, uint16_t) {}  // TODO(4)
inline void op_bvs(Cpu&, uint16_t) {}  // TODO(4)
inline void op_bcc(Cpu&, uint16_t) {}  // TODO(4)
inline void op_bcs(Cpu&, uint16_t) {}  // TODO(4)
inline void op_bne(Cpu&, uint16_t) {}  // TODO(4)
inline void op_beq(Cpu&, uint16_t) {}  // TODO(4)
//@LABS-END

//@LABS-BEGIN 5
//@LABS-SOLUTION
inline void op_jmp(Cpu& c, uint16_t ad) { c.pc = ad; }

// JSR shares mode_abs with JMP: by the time the op runs, pc sits just past
// the target, so the pushed return address is pc-1 (RTS adds the 1 back).
inline void op_jsr(Cpu& c, uint16_t ad) {
    ++c.cycles;  // internal cycle while juggling the return address
    const uint16_t ret = uint16_t(c.pc - 1);
    c.push(ret >> 8);
    c.push(ret & 0xFF);
    c.pc = ad;
}

inline void op_rts(Cpu& c, uint16_t) {
    c.cycles += 3;  // internal: dummy stack reads and the +1 fixup
    const uint16_t lo = c.pop();
    c.pc = uint16_t(lo | uint16_t(c.pop()) << 8) + 1;
}

inline void op_rti(Cpu& c, uint16_t) {
    c.cycles += 2;  // internal: dummy reads during vector assembly
    c.p = uint8_t((c.pop() & ~FB) | FU);
    const uint16_t lo = c.pop();
    c.pc = lo | uint16_t(c.pop()) << 8;
}

inline void op_brk(Cpu& c, uint16_t) {
    c.fetch8();  // padding byte: BRK is a 2-byte instruction
    c.push(c.pc >> 8);
    c.push(uint8_t(c.pc));
    c.push(uint8_t(c.p | FB | FU));  // B set distinguishes BRK from IRQ
    c.p |= FI;
    const uint16_t lo = c.read(0xFFFE);
    c.pc = lo | uint16_t(c.read(0xFFFF)) << 8;
}

inline void op_clc(Cpu& c, uint16_t) {
    ++c.cycles;  // implied ops dummy-read the next byte
    c.p &= ~FC;
}
inline void op_sec(Cpu& c, uint16_t) {
    ++c.cycles;  // implied ops dummy-read the next byte
    c.p |= FC;
}
inline void op_cli(Cpu& c, uint16_t) {
    ++c.cycles;  // implied ops dummy-read the next byte
    c.p &= ~FI;
}
inline void op_sei(Cpu& c, uint16_t) {
    ++c.cycles;  // implied ops dummy-read the next byte
    c.p |= FI;
}
inline void op_clv(Cpu& c, uint16_t) {
    ++c.cycles;  // implied ops dummy-read the next byte
    c.p &= ~FV;
}
inline void op_cld(Cpu& c, uint16_t) {
    ++c.cycles;  // implied ops dummy-read the next byte
    c.p &= ~FD;
}
inline void op_sed(Cpu& c, uint16_t) {
    ++c.cycles;  // implied ops dummy-read the next byte
    c.p |= FD;
}
inline void op_nop(Cpu& c, uint16_t) {
    ++c.cycles;  // implied ops dummy-read the next byte
}
//@LABS-STUB
// TODO(5): jumps and the stack flow group. JSR pushes pc-1 (mode_abs has
// already consumed the target); RTS pops and ADDS 1; RTI pops P first
// (B stripped) then PC; BRK pushes P with B set and vectors through
// $FFFE/$FFFF. Flag setters/clearers map 1:1 onto flag bits.
inline void op_jmp(Cpu&, uint16_t) {}  // TODO(5)
inline void op_jsr(Cpu&, uint16_t) {}  // TODO(5)
inline void op_rts(Cpu&, uint16_t) {}  // TODO(5)
inline void op_rti(Cpu&, uint16_t) {}  // TODO(5)
inline void op_brk(Cpu&, uint16_t) {}  // TODO(5)
inline void op_clc(Cpu&, uint16_t) {}  // TODO(5)
inline void op_sec(Cpu&, uint16_t) {}  // TODO(5)
inline void op_cli(Cpu&, uint16_t) {}  // TODO(5)
inline void op_sei(Cpu&, uint16_t) {}  // TODO(5)
inline void op_clv(Cpu&, uint16_t) {}  // TODO(5)
inline void op_cld(Cpu&, uint16_t) {}  // TODO(5)
inline void op_sed(Cpu&, uint16_t) {}  // TODO(5)
inline void op_nop(Cpu&, uint16_t) {}  // TODO(5)
//@LABS-END

// ---------------------------------------------------------------------------
// Decode table (official set; unofficial opcodes arrive in chapter 19).
// ---------------------------------------------------------------------------

enum class Penalty : uint8_t { None, OnCross, Always };

struct Entry {
    ModeFn mode = nullptr;
    OpFn op = nullptr;
    uint8_t base = 0;
    Penalty penalty = Penalty::None;
};

inline constexpr std::array<Entry, 256> kTable = [] {
    std::array<Entry, 256> t{};
    using P = Penalty;
    // Loads / stores
    t[0xA9] = {mode_imm, op_lda, 2};   t[0xA5] = {mode_zp, op_lda, 3};
    t[0xB5] = {mode_zpx, op_lda, 4, P::Always};   t[0xAD] = {mode_abs, op_lda, 4};
    t[0xBD] = {mode_absx, op_lda, 4, P::OnCross};
    t[0xB9] = {mode_absy, op_lda, 4, P::OnCross};
    t[0xA1] = {mode_izx, op_lda, 6, P::Always};   t[0xB1] = {mode_izy, op_lda, 5, P::OnCross};
    t[0xA2] = {mode_imm, op_ldx, 2};   t[0xA6] = {mode_zp, op_ldx, 3};
    t[0xB6] = {mode_zpy, op_ldx, 4, P::Always};   t[0xAE] = {mode_abs, op_ldx, 4};
    t[0xBE] = {mode_absy, op_ldx, 4, P::OnCross};
    t[0xA0] = {mode_imm, op_ldy, 2};   t[0xA4] = {mode_zp, op_ldy, 3};
    t[0xB4] = {mode_zpx, op_ldy, 4, P::Always};   t[0xAC] = {mode_abs, op_ldy, 4};
    t[0xBC] = {mode_absx, op_ldy, 4, P::OnCross};
    t[0x85] = {mode_zp, op_sta, 3};    t[0x95] = {mode_zpx, op_sta, 4, P::Always};
    t[0x8D] = {mode_abs, op_sta, 4};   t[0x9D] = {mode_absx, op_sta, 5, P::Always};
    t[0x99] = {mode_absy, op_sta, 5, P::Always};
    t[0x81] = {mode_izx, op_sta, 6, P::Always};   t[0x91] = {mode_izy, op_sta, 6, P::Always};
    t[0x86] = {mode_zp, op_stx, 3};    t[0x96] = {mode_zpy, op_stx, 4, P::Always};
    t[0x8E] = {mode_abs, op_stx, 4};
    t[0x84] = {mode_zp, op_sty, 3};    t[0x94] = {mode_zpx, op_sty, 4, P::Always};
    t[0x8C] = {mode_abs, op_sty, 4};
    // Transfers / stack
    t[0xAA] = {mode_imp, op_tax, 2};   t[0xA8] = {mode_imp, op_tay, 2};
    t[0x8A] = {mode_imp, op_txa, 2};   t[0x98] = {mode_imp, op_tya, 2};
    t[0xBA] = {mode_imp, op_tsx, 2};   t[0x9A] = {mode_imp, op_txs, 2};
    t[0x48] = {mode_imp, op_pha, 3};   t[0x08] = {mode_imp, op_php, 3};
    t[0x68] = {mode_imp, op_pla, 4};   t[0x28] = {mode_imp, op_plp, 4};
    // Logic
    t[0x29] = {mode_imm, op_and, 2};   t[0x25] = {mode_zp, op_and, 3};
    t[0x35] = {mode_zpx, op_and, 4, P::Always};   t[0x2D] = {mode_abs, op_and, 4};
    t[0x3D] = {mode_absx, op_and, 4, P::OnCross};
    t[0x21] = {mode_izx, op_and, 6, P::Always};   t[0x31] = {mode_izy, op_and, 5, P::OnCross};
    t[0x09] = {mode_imm, op_ora, 2};   t[0x05] = {mode_zp, op_ora, 3};
    t[0x15] = {mode_zpx, op_ora, 4, P::Always};   t[0x0D] = {mode_abs, op_ora, 4};
    t[0x19] = {mode_absy, op_ora, 4, P::OnCross};
    t[0x01] = {mode_izx, op_ora, 6, P::Always};   t[0x11] = {mode_izy, op_ora, 5, P::OnCross};
    t[0x49] = {mode_imm, op_eor, 2};   t[0x45] = {mode_zp, op_eor, 3};
    t[0x55] = {mode_zpx, op_eor, 4, P::Always};   t[0x4D] = {mode_abs, op_eor, 4};
    t[0x5D] = {mode_absx, op_eor, 4, P::OnCross};
    t[0x59] = {mode_absy, op_eor, 4, P::OnCross};   t[0x51] = {mode_izy, op_eor, 5, P::OnCross};    t[0x2C] = {mode_abs, op_bit, 4};
    // Arithmetic
    t[0x69] = {mode_imm, op_adc, 2};   t[0x65] = {mode_zp, op_adc, 3};   t[0x6D] = {mode_abs, op_adc, 4};
    t[0x7D] = {mode_absx, op_adc, 4, P::OnCross};
    t[0x79] = {mode_absy, op_adc, 4, P::OnCross};
    t[0x61] = {mode_izx, op_adc, 6, P::Always};   t[0x71] = {mode_izy, op_adc, 5, P::OnCross};
    t[0xE9] = {mode_imm, op_sbc, 2};   t[0xE5] = {mode_zp, op_sbc, 3};
    t[0xF5] = {mode_zpx, op_sbc, 4, P::Always};   t[0xED] = {mode_abs, op_sbc, 4};
    t[0xF9] = {mode_absy, op_sbc, 4, P::OnCross};
    t[0xE1] = {mode_izx, op_sbc, 6, P::Always};   t[0xF1] = {mode_izy, op_sbc, 5, P::OnCross};
    // Compares
    t[0xC9] = {mode_imm, op_cmp, 2};   t[0xC5] = {mode_zp, op_cmp, 3};
    t[0xD5] = {mode_zpx, op_cmp, 4, P::Always};   t[0xCD] = {mode_abs, op_cmp, 4};
    t[0xDD] = {mode_absx, op_cmp, 4, P::OnCross};
    t[0xD9] = {mode_absy, op_cmp, 4, P::OnCross};
    t[0xC1] = {mode_izx, op_cmp, 6, P::Always};
    t[0xE0] = {mode_imm, op_cpx, 2};   t[0xE4] = {mode_zp, op_cpx, 3};
    t[0xEC] = {mode_abs, op_cpx, 4};
    t[0xC0] = {mode_imm, op_cpy, 2};   t[0xC4] = {mode_zp, op_cpy, 3};
    t[0xCC] = {mode_abs, op_cpy, 4};
    // INC / DEC
    t[0xE6] = {mode_zp, op_inc, 5};    t[0xF6] = {mode_zpx, op_inc, 6, P::Always};
    t[0xEE] = {mode_abs, op_inc, 6};  
    t[0xC6] = {mode_zp, op_dec, 5};    t[0xD6] = {mode_zpx, op_dec, 6, P::Always};
    t[0xCE] = {mode_abs, op_dec, 6};
    t[0xE8] = {mode_imp, op_inx, 2};   t[0xC8] = {mode_imp, op_iny, 2};
    t[0xCA] = {mode_imp, op_dex, 2};   t[0x88] = {mode_imp, op_dey, 2};
    // Shifts
    t[0x0A] = {mode_acc, op_asl_a, 2};
    t[0x06] = {mode_zp, op_asl, 5};    t[0x16] = {mode_zpx, op_asl, 6, P::Always};
    t[0x0E] = {mode_abs, op_asl, 6};   t[0x1E] = {mode_absx, op_asl, 7, P::Always};
    t[0x4A] = {mode_acc, op_lsr_a, 2};
    t[0x46] = {mode_zp, op_lsr, 5};    t[0x56] = {mode_zpx, op_lsr, 6, P::Always};
    t[0x4E] = {mode_abs, op_lsr, 6};   t[0x5E] = {mode_absx, op_lsr, 7, P::Always};
    t[0x2A] = {mode_acc, op_rol_a, 2};
    t[0x26] = {mode_zp, op_rol, 5};    t[0x36] = {mode_zpx, op_rol, 6, P::Always};
    t[0x2E] = {mode_abs, op_rol, 6};   t[0x3E] = {mode_absx, op_rol, 7, P::Always};
    t[0x6A] = {mode_acc, op_ror_a, 2};
    t[0x66] = {mode_zp, op_ror, 5};
    t[0x6E] = {mode_abs, op_ror, 6};   t[0x7E] = {mode_absx, op_ror, 7, P::Always};
    // Branches
    t[0x10] = {mode_imp, op_bpl, 2};   t[0x30] = {mode_imp, op_bmi, 2};
    t[0x50] = {mode_imp, op_bvc, 2};   t[0x70] = {mode_imp, op_bvs, 2};
    t[0x90] = {mode_imp, op_bcc, 2};   t[0xB0] = {mode_imp, op_bcs, 2};
    t[0xD0] = {mode_imp, op_bne, 2};   t[0xF0] = {mode_imp, op_beq, 2};
    // Jumps / subroutines / interrupts
    t[0x4C] = {mode_abs, op_jmp, 3};   t[0x6C] = {mode_ind, op_jmp, 5};
    t[0x20] = {mode_abs, op_jsr, 6};   t[0x60] = {mode_imp, op_rts, 6};
    t[0x40] = {mode_imp, op_rti, 6};   t[0x00] = {mode_imp, op_brk, 7};
    // Flags / NOP
    t[0x18] = {mode_imp, op_clc, 2};   t[0x38] = {mode_imp, op_sec, 2};
    t[0x58] = {mode_imp, op_cli, 2};   t[0x78] = {mode_imp, op_sei, 2};
    t[0xB8] = {mode_imp, op_clv, 2};   t[0xD8] = {mode_imp, op_cld, 2};
    t[0xF8] = {mode_imp, op_sed, 2};   t[0xEA] = {mode_imp, op_nop, 2};
    return t;
}();

inline int step(Cpu& c) {
    if (c.halted || c.bus == nullptr) return 0;
    const uint64_t t0 = c.cycles;
    const Entry& e = kTable[c.fetch8()];
    if (!e.mode || !e.op) {
        c.halted = true;
        return int(c.cycles - t0);
    }
    uint16_t addr = 0;
    const bool crossed = e.mode(c, addr);
    switch (e.penalty) {
        case Penalty::OnCross:
            if (crossed) ++c.cycles;
            break;
        case Penalty::Always:
            ++c.cycles;
            break;
        case Penalty::None:
            break;
    }
    e.op(c, addr);
    return int(c.cycles - t0);
}

inline void run(Cpu& c, uint64_t max_instructions) {
    for (uint64_t i = 0; i < max_instructions && !c.halted; ++i) step(c);
}

}  // namespace nes6502
