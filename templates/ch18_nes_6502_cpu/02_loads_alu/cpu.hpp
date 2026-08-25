// nes6502.hpp — 6502 core with the {mode_fn, op_fn} dispatch table.
//
// Addressing modes are already implemented (see 01_addressing_modes); this
// exercise fills in opcode SEMANTICS as free functions and wires them into
// a 256-entry decode table. Cycle accounting: every bus access is one
// cycle; page-cross penalties are billed by step() from the mode's report.
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
    bool halted = false;  // set when an unimplemented/unofficial opcode runs

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

// ---------------------------------------------------------------------------
// Addressing modes (provided complete — these were chapter exercise 01).
// ---------------------------------------------------------------------------

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
    const uint16_t lo = c.read(ptr);
    const uint16_t hi = c.read(uint8_t(ptr + 1));
    out = lo | hi << 8;
    return false;
}
inline bool mode_izy(Cpu& c, uint16_t& out) {
    const uint8_t ptr = c.fetch8();
    const uint16_t base =
        c.read(ptr) | uint16_t(c.read(uint8_t(ptr + 1))) << 8;
    out = uint16_t(base + c.y);
    return (base & 0xFF00) != (out & 0xFF00);
}

// ---------------------------------------------------------------------------
// Opcode semantics — YOUR work in this exercise.
// ---------------------------------------------------------------------------

using OpFn = void (*)(Cpu&, uint16_t);

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline void op_lda(Cpu& c, uint16_t ad) { c.a = c.read(ad); c.set_zn(c.a); }
inline void op_ldx(Cpu& c, uint16_t ad) { c.x = c.read(ad); c.set_zn(c.x); }
inline void op_ldy(Cpu& c, uint16_t ad) { c.y = c.read(ad); c.set_zn(c.y); }
inline void op_sta(Cpu& c, uint16_t ad) { c.write(ad, c.a); }
inline void op_stx(Cpu& c, uint16_t ad) { c.write(ad, c.x); }
inline void op_sty(Cpu& c, uint16_t ad) { c.write(ad, c.y); }
//@LABS-STUB
// TODO(1): loads read memory into the register and set N/Z from the value;
// stores write the register OUT to memory (stores never touch flags).
inline void op_lda(Cpu&, uint16_t) {}  // TODO(1)
inline void op_ldx(Cpu&, uint16_t) {}  // TODO(1)
inline void op_ldy(Cpu&, uint16_t) {}  // TODO(1)
inline void op_sta(Cpu&, uint16_t) {}  // TODO(1)
inline void op_stx(Cpu&, uint16_t) {}  // TODO(1)
inline void op_sty(Cpu&, uint16_t) {}  // TODO(1)
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
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
}  // no flags on TXS!

inline void op_pha(Cpu& c, uint16_t) {
    ++c.cycles;  // internal: SP dummy decrement
    c.push(c.a);
}
inline void op_php(Cpu& c, uint16_t) {
    // The stacked copy always carries B=1 and the unused bit=1.
    c.push(uint8_t(c.p | FB | FU));
}
inline void op_pla(Cpu& c, uint16_t) {
    c.cycles += 2;  // internal: two dummy SP increments
    c.a = c.pop();
    c.set_zn(c.a);
}
inline void op_plp(Cpu& c, uint16_t) {
    c.cycles += 2;  // internal: two dummy SP increments
    // B never exists in the live P register; unused bit still reads as 1.
    c.p = uint8_t((c.pop() & ~FB) | FU);
}
//@LABS-STUB
// TODO(2): register transfers (TXS is the only one that skips N/Z) and the
// four stack ops. PHP pushes P with bits B and U forced high; PLP pops it
// with B forced low and U high.
inline void op_tax(Cpu&, uint16_t) {}  // TODO(2)
inline void op_tay(Cpu&, uint16_t) {}  // TODO(2)
inline void op_txa(Cpu&, uint16_t) {}  // TODO(2)
inline void op_tya(Cpu&, uint16_t) {}  // TODO(2)
inline void op_tsx(Cpu&, uint16_t) {}  // TODO(2)
inline void op_txs(Cpu&, uint16_t) {}  // TODO(2)
inline void op_pha(Cpu&, uint16_t) {}  // TODO(2)
inline void op_php(Cpu&, uint16_t) {}  // TODO(2)
inline void op_pla(Cpu&, uint16_t) {}  // TODO(2)
inline void op_plp(Cpu&, uint16_t) {}  // TODO(2)
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
inline void op_and(Cpu& c, uint16_t ad) {
    c.a &= c.read(ad);
    c.set_zn(c.a);
}
inline void op_ora(Cpu& c, uint16_t ad) {
    c.a |= c.read(ad);
    c.set_zn(c.a);
}
inline void op_eor(Cpu& c, uint16_t ad) {
    c.a ^= c.read(ad);
    c.set_zn(c.a);
}
inline void op_bit(Cpu& c, uint16_t ad) {
    // BIT is special: N and V come from the MEMORY operand bits 7/6,
    // while Z reflects A & m. The accumulator itself is untouched.
    const uint8_t m = c.read(ad);
    c.p = uint8_t((c.p & ~(FN | FV | FZ)) | (m & FN) |
                  ((m & FV) ? FV : 0) | ((c.a & m) == 0 ? FZ : 0));
}
//@LABS-STUB
// TODO(3): logic group. AND/OR/EOR update A and set N/Z. BIT copies the
// operand's bit 7 into N and bit 6 into V, and sets Z from A & m.
inline void op_and(Cpu&, uint16_t) {}  // TODO(3)
inline void op_ora(Cpu&, uint16_t) {}  // TODO(3)
inline void op_eor(Cpu&, uint16_t) {}  // TODO(3)
inline void op_bit(Cpu&, uint16_t) {}  // TODO(3)
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// Shared binary-adder path used by both ADC and SBC: SBC is ADC of the
// bitwise complement with the same carry-in (a - m - !C == a + ~m + C).
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
//@LABS-STUB
// TODO(4): ADC adds operand + carry-in, sets carry-out when the sum
// exceeds $FF and V when signed overflow occurs (~(a^m)&(a^sum)&$80).
// SBC reuses the exact same path with the complemented operand.
inline void op_adc(Cpu&, uint16_t) {}  // TODO(4)
inline void op_sbc(Cpu&, uint16_t) {}  // TODO(4)
//@LABS-END

// ---------------------------------------------------------------------------
// Decode table.
// ---------------------------------------------------------------------------

enum class Penalty : uint8_t { None, OnCross, Always };

struct Entry {
    ModeFn mode = nullptr;
    OpFn op = nullptr;
    uint8_t base = 0;  // documented base cycles (asserted in tests)
    Penalty penalty = Penalty::None;
};

inline constexpr std::array<Entry, 256> kTable = [] {
    std::array<Entry, 256> t{};
    // Loads / stores
    t[0xA9] = {mode_imm, op_lda, 2};   t[0xA5] = {mode_zp, op_lda, 3};
    t[0xB5] = {mode_zpx, op_lda, 4, Penalty::Always};   t[0xAD] = {mode_abs, op_lda, 4};
    t[0xBD] = {mode_absx, op_lda, 4, Penalty::OnCross};
    t[0xB9] = {mode_absy, op_lda, 4, Penalty::OnCross};
    t[0xA1] = {mode_izx, op_lda, 6, Penalty::Always};   t[0xB1] = {mode_izy, op_lda, 5, Penalty::OnCross};
    t[0xA2] = {mode_imm, op_ldx, 2};   t[0xA6] = {mode_zp, op_ldx, 3};
    t[0xB6] = {mode_zpy, op_ldx, 4, Penalty::Always};   t[0xAE] = {mode_abs, op_ldx, 4};
    t[0xBE] = {mode_absy, op_ldx, 4, Penalty::OnCross};
    t[0xA0] = {mode_imm, op_ldy, 2};   t[0xA4] = {mode_zp, op_ldy, 3};
    t[0xB4] = {mode_zpx, op_ldy, 4, Penalty::Always};   t[0xAC] = {mode_abs, op_ldy, 4};
    t[0xBC] = {mode_absx, op_ldy, 4, Penalty::OnCross};
    t[0x85] = {mode_zp, op_sta, 3};    t[0x95] = {mode_zpx, op_sta, 4, Penalty::Always};
    t[0x8D] = {mode_abs, op_sta, 4};   t[0x9D] = {mode_absx, op_sta, 5, Penalty::Always};
    t[0x99] = {mode_absy, op_sta, 5, Penalty::Always};
    t[0x81] = {mode_izx, op_sta, 6, Penalty::Always};   t[0x91] = {mode_izy, op_sta, 6, Penalty::Always};
    t[0x86] = {mode_zp, op_stx, 3};    t[0x96] = {mode_zpy, op_stx, 4, Penalty::Always};
    t[0x8E] = {mode_abs, op_stx, 4};
    t[0x84] = {mode_zp, op_sty, 3};    t[0x94] = {mode_zpx, op_sty, 4, Penalty::Always};
    t[0x8C] = {mode_abs, op_sty, 4};
    // Transfers / stack
    t[0xAA] = {mode_imp, op_tax, 2};   t[0xA8] = {mode_imp, op_tay, 2};
    t[0x8A] = {mode_imp, op_txa, 2};   t[0x98] = {mode_imp, op_tya, 2};
    t[0xBA] = {mode_imp, op_tsx, 2};   t[0x9A] = {mode_imp, op_txs, 2};
    t[0x48] = {mode_imp, op_pha, 3};   t[0x08] = {mode_imp, op_php, 3};
    t[0x68] = {mode_imp, op_pla, 4};   t[0x28] = {mode_imp, op_plp, 4};
    // Logic
    t[0x29] = {mode_imm, op_and, 2};   t[0x25] = {mode_zp, op_and, 3};
    t[0x35] = {mode_zpx, op_and, 4, Penalty::Always};   t[0x2D] = {mode_abs, op_and, 4};
    t[0x3D] = {mode_absx, op_and, 4, Penalty::OnCross};
    t[0x21] = {mode_izx, op_and, 6, Penalty::Always};   t[0x31] = {mode_izy, op_and, 5, Penalty::OnCross};
    t[0x09] = {mode_imm, op_ora, 2};   t[0x05] = {mode_zp, op_ora, 3};
    t[0x15] = {mode_zpx, op_ora, 4, Penalty::Always};   t[0x0D] = {mode_abs, op_ora, 4};
    t[0x19] = {mode_absy, op_ora, 4, Penalty::OnCross};
    t[0x01] = {mode_izx, op_ora, 6, Penalty::Always};   t[0x11] = {mode_izy, op_ora, 5, Penalty::OnCross};
    t[0x49] = {mode_imm, op_eor, 2};   t[0x45] = {mode_zp, op_eor, 3};
    t[0x55] = {mode_zpx, op_eor, 4, Penalty::Always};   t[0x4D] = {mode_abs, op_eor, 4};
    t[0x5D] = {mode_absx, op_eor, 4, Penalty::OnCross};
    t[0x59] = {mode_absy, op_eor, 4, Penalty::OnCross};   t[0x51] = {mode_izy, op_eor, 5, Penalty::OnCross};    t[0x2C] = {mode_abs, op_bit, 4};
    // Arithmetic
    t[0x69] = {mode_imm, op_adc, 2};   t[0x65] = {mode_zp, op_adc, 3};   t[0x6D] = {mode_abs, op_adc, 4};
    t[0x7D] = {mode_absx, op_adc, 4, Penalty::OnCross};
    t[0x79] = {mode_absy, op_adc, 4, Penalty::OnCross};
    t[0x61] = {mode_izx, op_adc, 6, Penalty::Always};   t[0x71] = {mode_izy, op_adc, 5, Penalty::OnCross};
    t[0xE9] = {mode_imm, op_sbc, 2};   t[0xE5] = {mode_zp, op_sbc, 3};
    t[0xF5] = {mode_zpx, op_sbc, 4, Penalty::Always};   t[0xED] = {mode_abs, op_sbc, 4};
    t[0xF9] = {mode_absy, op_sbc, 4, Penalty::OnCross};
    t[0xE1] = {mode_izx, op_sbc, 6, Penalty::Always};   t[0xF1] = {mode_izy, op_sbc, 5, Penalty::OnCross};
    return t;
}();

/// Execute one instruction; returns cycles consumed. Unimplemented or
/// unofficial opcodes halt the CPU (they arrive in later chapters).
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
            if (crossed) ++c.cycles;  // the speculative dummy access
            break;
        case Penalty::Always:
            ++c.cycles;  // stores/RMW index first, penalty is unconditional
            break;
        case Penalty::None:
            break;
    }
    e.op(c, addr);
    return int(c.cycles - t0);
}

/// Run until halted or the instruction budget runs out.
inline void run(Cpu& c, uint64_t max_instructions) {
    for (uint64_t i = 0; i < max_instructions && !c.halted; ++i) step(c);
}

}  // namespace nes6502
