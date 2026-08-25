// nes6502.hpp — 6502 core, challenge: match the reference log.
//
// The complete accuracy core; your job is the final trace-line
// Built on the finished ch18 core (complete official set, access-counted
// cycles) — see templates/ch18_nes_6502_cpu. Chapter 19 layers NES-level
// accuracy on top: RESET/BRK/IRQ/NMI vector flows with the stacked-B rule,
// dummy bus accesses (RMW double-write, speculative indexed reads),
// a documented unofficial-opcode subset, and nestest-style trace logging.
#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

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

struct Cpu;

// A single bus transaction, recorded by RecordingBus so tests can observe
// dummy reads/writes that never show up in registers.
struct Access {
    uint16_t addr;
    uint8_t value;
    bool write;
};

class RecordingBus final : public Bus {
public:
    std::array<uint8_t, 0x10000> mem{};
    std::vector<Access> log;
    uint8_t read(uint16_t addr) override {
        log.push_back({addr, mem[addr], false});
        return mem[addr];
    }
    void write(uint16_t addr, uint8_t v) override {
        log.push_back({addr, v, true});
        mem[addr] = v;
    }
};

struct Cpu {
    uint8_t a = 0, x = 0, y = 0;
    uint8_t sp = 0xFD;
    uint8_t p = FU | FI;
    uint16_t pc = 0;
    uint64_t cycles = 0;
    Bus* bus = nullptr;
    bool halted = false;
    // Interrupt lines. NMI is edge-latched (level change -> pending);
    // IRQ is level-sensitive and masked by the I flag.
    bool nmi_line = false, irq_line = false;
    bool nmi_pending = false;
    // Indexed addressing stores the NOT-fixed-up address here so step()
    // can perform the hardware's dummy read at the wrong-page address.
    uint16_t fixup_addr = 0;

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

/// Drive the NMI input line. The 6502 latches the request on the QUIET ->
/// HIGH transition: holding the line asserted does not stack up extra
/// requests, and a latched request stays set until serviced.
inline void set_nmi_line(Cpu& c, bool high) {
    if (high && !c.nmi_line) c.nmi_pending = true;
    c.nmi_line = high;
}

// ---- addressing modes (inherited complete from ch18) ----------------------

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
    // The chip performs its (speculative) access at the un-fixed-up
    // address: base's page with the low byte of the sum.
    c.fixup_addr = uint16_t((base & 0xFF00) | (out & 0x00FF));
    return (base & 0xFF00) != (out & 0xFF00);
}
inline bool mode_absy(Cpu& c, uint16_t& out) {
    const uint16_t base = c.fetch16();
    out = uint16_t(base + c.y);
    c.fixup_addr = uint16_t((base & 0xFF00) | (out & 0x00FF));
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
    c.fixup_addr = uint16_t((base & 0xFF00) | (out & 0x00FF));
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

// ---- loads/stores/transfers/logic/arith/compares (inherited complete) -----

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

// CMP is a subtraction whose only survivors are C (reg >= m), Z and N.
inline void cmp_reg(Cpu& c, uint8_t reg, uint8_t m) {
    c.p = uint8_t((c.p & ~FC) | (reg >= m ? FC : 0));
    c.set_zn(uint8_t(reg - m));
}

inline void op_cmp(Cpu& c, uint16_t ad) { cmp_reg(c, c.a, c.read(ad)); }
inline void op_cpx(Cpu& c, uint16_t ad) { cmp_reg(c, c.x, c.read(ad)); }
inline void op_cpy(Cpu& c, uint16_t ad) { cmp_reg(c, c.y, c.read(ad)); }

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

// ---------------------------------------------------------------------------
// Interrupt flows (chapter 19).
//
// All entries push PCH then PCL then P; the ONLY difference between BRK
// and hardware IRQ is the state of the B bit in the pushed copy of P
// (BRK sets it, IRQ/NMI leave it clear). NMI vectors through $FFFA and
// ignores the I flag; IRQ and BRK share $FFFE.
// ---------------------------------------------------------------------------

// ---- shifts / branches / jumps / flags (inherited from ch18 03) -------

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

// BRK is a software interrupt: the padding byte is fetched (BRK is a
// 2-byte instruction, so the pushed PC points past it), P goes on the
// stack WITH the B bit set, I is raised, and control reaches $FFFE.
inline void op_brk(Cpu& c, uint16_t) {
    c.fetch8();                       // padding byte
    c.push(c.pc >> 8);                // PCH
    c.push(uint8_t(c.pc));            // PCL
    c.push(uint8_t(c.p | FB | FU));   // B SET distinguishes BRK from IRQ
    c.p |= FI;
    const uint16_t lo = c.read(0xFFFE);
    c.pc = lo | uint16_t(c.read(0xFFFF)) << 8;
}

/// Hardware reset: S decrements by 3, I is set, PC comes from $FFFC.
/// Billed as the official 7 cycles (two vector reads + 5 internal).
inline void reset(Cpu& c) {
    c.sp = uint8_t(c.sp - 3);
    c.p = uint8_t((c.p & ~(FB | FD)) | FI | FU);
    c.nmi_pending = false;
    const uint16_t lo = c.read(0xFFFC);
    c.pc = lo | uint16_t(c.read(0xFFFD)) << 8;
    c.cycles += 5;
}
/// Shared push/vector sequence. vector_lo selects $FFFA (NMI) or $FFFE
/// (IRQ family). The stacked P always carries B CLEAR for hardware flows;
/// two dummy opcode reads happen before anything lands on the stack.
inline void service_nmi_impl(Cpu& c, uint16_t vector_lo) {
    c.read(c.pc);  // two dummy opcode reads; PC does not advance
    c.read(c.pc);
    c.push(c.pc >> 8);
    c.push(uint8_t(c.pc));
    c.push(uint8_t((c.p & ~FB) | FU));  // B CLEAR distinguishes hw ints
    c.p |= FI;
    const uint16_t lo = c.read(vector_lo);
    c.pc = lo | uint16_t(c.read(uint16_t(vector_lo + 1))) << 8;
}

/// Non-maskable: ignores the I flag, 7 cycles ($FFFA).
inline void service_nmi(Cpu& c) { service_nmi_impl(c, 0xFFFA); }

/// Maskable: only polled when I is clear, 7 cycles ($FFFE).
inline void service_irq(Cpu& c) { service_nmi_impl(c, 0xFFFE); }
// ---------------------------------------------------------------------------
// Unofficial opcode subset (documented choices; every row below is wired
// into the decode table like an official opcode).
//
//   NOPs      : 1A 3A 5A 7A DA FA (implied), 80 82 89 C2 E2 (imm),
//               04 44 64 (zp), 14 34 54 74 D4 F4 (zp,X), 0C (abs),
//               1C 3C 5C 7C DC FC (abs,X)
//   LAX       : A7 B7 AF BF A3 B3     A = X = mem
//   SAX       : 87 97 8F 83           mem = A & X (no flags)
//   DCP       : C7 D7 CF DF DB D3     DEC mem, then CMP A
//   ISB (ISC) : E7 F7 EF FF FB F3     INC mem, then SBC
//   SLO       : 07 17 0F 1F 1B 13     ASL mem, then ORA A
//   RLA       : 27 37 2F 3F 3B 33     ROL mem, then AND A
// ---------------------------------------------------------------------------

inline void op_nop_u(Cpu& c, uint16_t) {
    ++c.cycles;  // implied NOPs dummy-read the next byte (official total 2)
}
// Operand-bearing NOPs really do READ their effective address on the die;
// going through the normal read path is what produces the official 3-cycle
// zp / 4-cycle abs / 5-cycle crossed abs,X timings.
inline void op_nop_mem(Cpu& c, uint16_t ad) { c.read(ad); }

inline void op_lax(Cpu& c, uint16_t ad) {
    const uint8_t m = c.read(ad);
    c.a = m;
    c.x = m;
    c.set_zn(m);
}
inline void op_sax(Cpu& c, uint16_t ad) { c.write(ad, uint8_t(c.a & c.x)); }

inline void op_dcp(Cpu& c, uint16_t ad) { cmp_reg(c, c.a, rmw(c, ad, raw_dec)); }
inline void op_isb(Cpu& c, uint16_t ad) { adc_raw(c, uint8_t(~rmw(c, ad, raw_inc))); }
inline void op_slo(Cpu& c, uint16_t ad) {
    c.a |= rmw(c, ad, raw_asl);
    c.set_zn(c.a);
}
inline void op_rla(Cpu& c, uint16_t ad) {
    c.a &= rmw(c, ad, raw_rol);
    c.set_zn(c.a);
}

// ---------------------------------------------------------------------------
// Decode table (full official set — including the ten combinations ch18's
// coding test added — plus the unofficial subset).
// ---------------------------------------------------------------------------

// OnCross      : one extra bus READ at fixup_addr when a page was crossed
//                (the 6502's speculative access at the un-fixed-up address)
// IndexedAlways : same dummy read, but unconditional (indexed stores/RMW)
// Internal     : one cycle with no observable access (zp,X / zp,Y / (zp,X)
//                pointer arithmetic on the real chip)
enum class Penalty : uint8_t { None, OnCross, IndexedAlways, Internal };

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
    t[0xB5] = {mode_zpx, op_lda, 4, P::Internal};   t[0xAD] = {mode_abs, op_lda, 4};
    t[0xBD] = {mode_absx, op_lda, 4, P::OnCross};
    t[0xB9] = {mode_absy, op_lda, 4, P::OnCross};
    t[0xA1] = {mode_izx, op_lda, 6, P::Internal};   t[0xB1] = {mode_izy, op_lda, 5, P::OnCross};
    t[0xA2] = {mode_imm, op_ldx, 2};   t[0xA6] = {mode_zp, op_ldx, 3};
    t[0xB6] = {mode_zpy, op_ldx, 4, P::Internal};   t[0xAE] = {mode_abs, op_ldx, 4};
    t[0xBE] = {mode_absy, op_ldx, 4, P::OnCross};
    t[0xA0] = {mode_imm, op_ldy, 2};   t[0xA4] = {mode_zp, op_ldy, 3};
    t[0xB4] = {mode_zpx, op_ldy, 4, P::Internal};   t[0xAC] = {mode_abs, op_ldy, 4};
    t[0xBC] = {mode_absx, op_ldy, 4, P::OnCross};
    t[0x85] = {mode_zp, op_sta, 3};    t[0x95] = {mode_zpx, op_sta, 4, P::Internal};
    t[0x8D] = {mode_abs, op_sta, 4};   t[0x9D] = {mode_absx, op_sta, 5, P::IndexedAlways};
    t[0x99] = {mode_absy, op_sta, 5, P::IndexedAlways};
    t[0x81] = {mode_izx, op_sta, 6, P::Internal};   t[0x91] = {mode_izy, op_sta, 6, P::IndexedAlways};
    t[0x86] = {mode_zp, op_stx, 3};    t[0x96] = {mode_zpy, op_stx, 4, P::Internal};
    t[0x8E] = {mode_abs, op_stx, 4};
    t[0x84] = {mode_zp, op_sty, 3};    t[0x94] = {mode_zpx, op_sty, 4, P::Internal};
    t[0x8C] = {mode_abs, op_sty, 4};
    // Transfers / stack
    t[0xAA] = {mode_imp, op_tax, 2};   t[0xA8] = {mode_imp, op_tay, 2};
    t[0x8A] = {mode_imp, op_txa, 2};   t[0x98] = {mode_imp, op_tya, 2};
    t[0xBA] = {mode_imp, op_tsx, 2};   t[0x9A] = {mode_imp, op_txs, 2};
    t[0x48] = {mode_imp, op_pha, 3};   t[0x08] = {mode_imp, op_php, 3};
    t[0x68] = {mode_imp, op_pla, 4};   t[0x28] = {mode_imp, op_plp, 4};
    // Logic
    t[0x29] = {mode_imm, op_and, 2};   t[0x25] = {mode_zp, op_and, 3};
    t[0x35] = {mode_zpx, op_and, 4, P::Internal};   t[0x2D] = {mode_abs, op_and, 4};
    t[0x3D] = {mode_absx, op_and, 4, P::OnCross};
    t[0x39] = {mode_absy, op_and, 4, P::OnCross};
    t[0x21] = {mode_izx, op_and, 6, P::Internal};   t[0x31] = {mode_izy, op_and, 5, P::OnCross};
    t[0x09] = {mode_imm, op_ora, 2};   t[0x05] = {mode_zp, op_ora, 3};
    t[0x15] = {mode_zpx, op_ora, 4, P::Internal};   t[0x0D] = {mode_abs, op_ora, 4};
    t[0x1D] = {mode_absx, op_ora, 4, P::OnCross};
    t[0x19] = {mode_absy, op_ora, 4, P::OnCross};
    t[0x01] = {mode_izx, op_ora, 6, P::Internal};   t[0x11] = {mode_izy, op_ora, 5, P::OnCross};
    t[0x49] = {mode_imm, op_eor, 2};   t[0x45] = {mode_zp, op_eor, 3};
    t[0x55] = {mode_zpx, op_eor, 4, P::Internal};   t[0x4D] = {mode_abs, op_eor, 4};
    t[0x5D] = {mode_absx, op_eor, 4, P::OnCross};
    t[0x59] = {mode_absy, op_eor, 4, P::OnCross};
    t[0x41] = {mode_izx, op_eor, 6, P::Internal};
    t[0x51] = {mode_izy, op_eor, 5, P::OnCross};
    t[0x24] = {mode_zp, op_bit, 3};    t[0x2C] = {mode_abs, op_bit, 4};
    // Arithmetic
    t[0x69] = {mode_imm, op_adc, 2};   t[0x65] = {mode_zp, op_adc, 3};
    t[0x75] = {mode_zpx, op_adc, 4, P::Internal};   t[0x6D] = {mode_abs, op_adc, 4};
    t[0x7D] = {mode_absx, op_adc, 4, P::OnCross};
    t[0x79] = {mode_absy, op_adc, 4, P::OnCross};
    t[0x61] = {mode_izx, op_adc, 6, P::Internal};   t[0x71] = {mode_izy, op_adc, 5, P::OnCross};
    t[0xE9] = {mode_imm, op_sbc, 2};   t[0xE5] = {mode_zp, op_sbc, 3};
    t[0xF5] = {mode_zpx, op_sbc, 4, P::Internal};   t[0xED] = {mode_abs, op_sbc, 4};
    t[0xFD] = {mode_absx, op_sbc, 4, P::OnCross};
    t[0xF9] = {mode_absy, op_sbc, 4, P::OnCross};
    t[0xE1] = {mode_izx, op_sbc, 6, P::Internal};   t[0xF1] = {mode_izy, op_sbc, 5, P::OnCross};
    // Compares
    t[0xC9] = {mode_imm, op_cmp, 2};   t[0xC5] = {mode_zp, op_cmp, 3};
    t[0xD5] = {mode_zpx, op_cmp, 4, P::Internal};   t[0xCD] = {mode_abs, op_cmp, 4};
    t[0xDD] = {mode_absx, op_cmp, 4, P::OnCross};
    t[0xD9] = {mode_absy, op_cmp, 4, P::OnCross};
    t[0xC1] = {mode_izx, op_cmp, 6, P::Internal};
    t[0xD1] = {mode_izy, op_cmp, 5, P::OnCross};
    t[0xE0] = {mode_imm, op_cpx, 2};   t[0xE4] = {mode_zp, op_cpx, 3};
    t[0xEC] = {mode_abs, op_cpx, 4};
    t[0xC0] = {mode_imm, op_cpy, 2};   t[0xC4] = {mode_zp, op_cpy, 3};
    t[0xCC] = {mode_abs, op_cpy, 4};
    // INC / DEC
    t[0xE6] = {mode_zp, op_inc, 5};    t[0xF6] = {mode_zpx, op_inc, 6, P::Internal};
    t[0xEE] = {mode_abs, op_inc, 6};   t[0xFE] = {mode_absx, op_inc, 7, P::IndexedAlways};
    t[0xC6] = {mode_zp, op_dec, 5};    t[0xD6] = {mode_zpx, op_dec, 6, P::Internal};
    t[0xCE] = {mode_abs, op_dec, 6};   t[0xDE] = {mode_absx, op_dec, 7, P::IndexedAlways};
    t[0xE8] = {mode_imp, op_inx, 2};   t[0xC8] = {mode_imp, op_iny, 2};
    t[0xCA] = {mode_imp, op_dex, 2};   t[0x88] = {mode_imp, op_dey, 2};
    // Shifts
    t[0x0A] = {mode_acc, op_asl_a, 2};
    t[0x06] = {mode_zp, op_asl, 5};    t[0x16] = {mode_zpx, op_asl, 6, P::Internal};
    t[0x0E] = {mode_abs, op_asl, 6};   t[0x1E] = {mode_absx, op_asl, 7, P::IndexedAlways};
    t[0x4A] = {mode_acc, op_lsr_a, 2};
    t[0x46] = {mode_zp, op_lsr, 5};    t[0x56] = {mode_zpx, op_lsr, 6, P::Internal};
    t[0x4E] = {mode_abs, op_lsr, 6};   t[0x5E] = {mode_absx, op_lsr, 7, P::IndexedAlways};
    t[0x2A] = {mode_acc, op_rol_a, 2};
    t[0x26] = {mode_zp, op_rol, 5};    t[0x36] = {mode_zpx, op_rol, 6, P::Internal};
    t[0x2E] = {mode_abs, op_rol, 6};   t[0x3E] = {mode_absx, op_rol, 7, P::IndexedAlways};
    t[0x6A] = {mode_acc, op_ror_a, 2};
    t[0x66] = {mode_zp, op_ror, 5};    t[0x76] = {mode_zpx, op_ror, 6, P::Internal};
    t[0x6E] = {mode_abs, op_ror, 6};   t[0x7E] = {mode_absx, op_ror, 7, P::IndexedAlways};
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
    // Unofficial subset
    t[0x1A] = {mode_imp, op_nop_u, 2}; t[0x3A] = {mode_imp, op_nop_u, 2};
    t[0x5A] = {mode_imp, op_nop_u, 2}; t[0x7A] = {mode_imp, op_nop_u, 2};
    t[0xDA] = {mode_imp, op_nop_u, 2}; t[0xFA] = {mode_imp, op_nop_u, 2};
    t[0x80] = {mode_imm, op_nop_mem, 2}; t[0x82] = {mode_imm, op_nop_mem, 2};
    t[0x89] = {mode_imm, op_nop_mem, 2}; t[0xC2] = {mode_imm, op_nop_mem, 2};
    t[0xE2] = {mode_imm, op_nop_mem, 2};
    t[0x04] = {mode_zp, op_nop_mem, 3};    t[0x44] = {mode_zp, op_nop_mem, 3};
    t[0x64] = {mode_zp, op_nop_mem, 3};    t[0x0C] = {mode_abs, op_nop_mem, 4};
    t[0x14] = {mode_zpx, op_nop_mem, 4, P::Internal};
    t[0x34] = {mode_zpx, op_nop_mem, 4, P::Internal};
    t[0x54] = {mode_zpx, op_nop_mem, 4, P::Internal};
    t[0x74] = {mode_zpx, op_nop_mem, 4, P::Internal};
    t[0xD4] = {mode_zpx, op_nop_mem, 4, P::Internal};
    t[0xF4] = {mode_zpx, op_nop_mem, 4, P::Internal};
    t[0x1C] = {mode_absx, op_nop_mem, 4, P::OnCross};
    t[0x3C] = {mode_absx, op_nop_mem, 4, P::OnCross};
    t[0x5C] = {mode_absx, op_nop_mem, 4, P::OnCross};
    t[0x7C] = {mode_absx, op_nop_mem, 4, P::OnCross};
    t[0xDC] = {mode_absx, op_nop_mem, 4, P::OnCross};
    t[0xFC] = {mode_absx, op_nop_mem, 4, P::OnCross};
    t[0xA7] = {mode_zp, op_lax, 3};    t[0xB7] = {mode_zpy, op_lax, 4, P::Internal};
    t[0xAF] = {mode_abs, op_lax, 4};   t[0xBF] = {mode_absy, op_lax, 4, P::OnCross};
    t[0xA3] = {mode_izx, op_lax, 6, P::Internal};
    t[0xB3] = {mode_izy, op_lax, 5, P::OnCross};
    t[0x87] = {mode_zp, op_sax, 3};    t[0x97] = {mode_zpy, op_sax, 4, P::Internal};
    t[0x8F] = {mode_abs, op_sax, 4};   t[0x83] = {mode_izx, op_sax, 6, P::Internal};
    t[0xC7] = {mode_zp, op_dcp, 5};    t[0xD7] = {mode_zpx, op_dcp, 6, P::Internal};
    t[0xCF] = {mode_abs, op_dcp, 6};   t[0xDF] = {mode_absx, op_dcp, 7, P::IndexedAlways};
    t[0xDB] = {mode_absy, op_dcp, 7, P::IndexedAlways};
    t[0xD3] = {mode_izy, op_dcp, 8, P::IndexedAlways};
    t[0xE7] = {mode_zp, op_isb, 5};    t[0xF7] = {mode_zpx, op_isb, 6, P::Internal};
    t[0xEF] = {mode_abs, op_isb, 6};   t[0xFF] = {mode_absx, op_isb, 7, P::IndexedAlways};
    t[0xFB] = {mode_absy, op_isb, 7, P::IndexedAlways};
    t[0xF3] = {mode_izy, op_isb, 8, P::IndexedAlways};
    t[0x07] = {mode_zp, op_slo, 5};    t[0x17] = {mode_zpx, op_slo, 6, P::Internal};
    t[0x0F] = {mode_abs, op_slo, 6};   t[0x1F] = {mode_absx, op_slo, 7, P::IndexedAlways};
    t[0x1B] = {mode_absy, op_slo, 7, P::IndexedAlways};
    t[0x13] = {mode_izy, op_slo, 8, P::IndexedAlways};
    t[0x27] = {mode_zp, op_rla, 5};    t[0x37] = {mode_zpx, op_rla, 6, P::Internal};
    t[0x2F] = {mode_abs, op_rla, 6};   t[0x3F] = {mode_absx, op_rla, 7, P::IndexedAlways};
    t[0x3B] = {mode_absy, op_rla, 7, P::IndexedAlways};
    t[0x33] = {mode_izy, op_rla, 8, P::IndexedAlways};
    return t;
}();

inline int step(Cpu& c) {
    if (c.halted || c.bus == nullptr) return 0;
    const uint64_t t0 = c.cycles;
    // Poll point: between instructions (the real chip polls during the
    // second-to-last cycle; boundary polling is our documented
    // simplification and shifts interrupts by at most one instruction).
    if (c.nmi_pending) {
        c.nmi_pending = false;  // edge-latched: exactly one service
        service_nmi(c);
        return int(c.cycles - t0);
    }
    if (c.irq_line && !(c.p & FI)) {
        service_irq(c);
        return int(c.cycles - t0);
    }
    const Entry& e = kTable[c.fetch8()];
    if (!e.mode || !e.op) {
        c.halted = true;
        return int(c.cycles - t0);
    }
    uint16_t addr = 0;
    const bool crossed = e.mode(c, addr);
    switch (e.penalty) {
        case Penalty::OnCross:
            if (crossed) c.read(c.fixup_addr);  // dummy read, billed by read()
            break;
        case Penalty::IndexedAlways:
            c.read(c.fixup_addr);  // speculative read happens every time
            break;
        case Penalty::Internal:
            ++c.cycles;  // no observable access on the bus
            break;
        case Penalty::None:
            break;
    }
    e.op(c, addr);
    return int(c.cycles - t0);
}
/// Run up to max_instructions steps.
inline void run(Cpu& c, uint64_t max_instructions) {
    for (uint64_t i = 0; i < max_instructions && !c.halted; ++i) step(c);
}

// ---------------------------------------------------------------------------
// nestest-style trace logging (chapter 19).
//
// Line shape (course variant of the nestest log; no PPU column because our
// flat-RAM rig has no PPU):
//
//   0600  A9 05      LDA #$05   A:05 X:00 Y:00 P:24 SP:FD CYC:7
//
//   PC     %04X, uppercase
//   bytes  opcode + operand slots "XX " each; unused slots are blank "   "
//   text   disassembly, left-justified in 10 columns
//   regs   A/X/Y/P/SP as %02X, CYC = cumulative cycles AFTER the step
//
// Usage pattern: snapshot pc, capture a TraceRow BEFORE stepping (operand
// bytes must be peeked before the instruction can rewrite them), step,
// then call trace_line() for the finished line.
// ---------------------------------------------------------------------------

struct TraceRow {
    uint16_t pc = 0;
    uint8_t op = 0, b1 = 0, b2 = 0;
    std::string text;
};

// Mnemonic per opcode; a null row renders as "???" (length 1, no operands).
inline constexpr std::array<const char*, 256> kMnemonic = [] {
    std::array<const char*, 256> m{};
    m[0xA9] = "LDA"; m[0xA5] = "LDA"; m[0xB5] = "LDA"; m[0xAD] = "LDA";
    m[0xBD] = "LDA"; m[0xB9] = "LDA"; m[0xA1] = "LDA"; m[0xB1] = "LDA";
    m[0xA2] = "LDX"; m[0xA6] = "LDX"; m[0xB6] = "LDX"; m[0xAE] = "LDX";
    m[0xBE] = "LDX";
    m[0xA0] = "LDY"; m[0xA4] = "LDY"; m[0xB4] = "LDY"; m[0xAC] = "LDY";
    m[0xBC] = "LDY";
    m[0x85] = "STA"; m[0x95] = "STA"; m[0x8D] = "STA"; m[0x9D] = "STA";
    m[0x99] = "STA"; m[0x81] = "STA"; m[0x91] = "STA";
    m[0x86] = "STX"; m[0x96] = "STX"; m[0x8E] = "STX";
    m[0x84] = "STY"; m[0x94] = "STY"; m[0x8C] = "STY";
    m[0xAA] = "TAX"; m[0xA8] = "TAY"; m[0x8A] = "TXA"; m[0x98] = "TYA";
    m[0xBA] = "TSX"; m[0x9A] = "TXS";
    m[0x48] = "PHA"; m[0x08] = "PHP"; m[0x68] = "PLA"; m[0x28] = "PLP";
    m[0x29] = "AND"; m[0x25] = "AND"; m[0x35] = "AND"; m[0x2D] = "AND";
    m[0x3D] = "AND"; m[0x39] = "AND"; m[0x21] = "AND"; m[0x31] = "AND";
    m[0x09] = "ORA"; m[0x05] = "ORA"; m[0x15] = "ORA"; m[0x0D] = "ORA";
    m[0x1D] = "ORA"; m[0x19] = "ORA"; m[0x01] = "ORA"; m[0x11] = "ORA";
    m[0x49] = "EOR"; m[0x45] = "EOR"; m[0x55] = "EOR"; m[0x4D] = "EOR";
    m[0x5D] = "EOR"; m[0x59] = "EOR"; m[0x41] = "EOR"; m[0x51] = "EOR";
    m[0x24] = "BIT"; m[0x2C] = "BIT";
    m[0x69] = "ADC"; m[0x65] = "ADC"; m[0x75] = "ADC"; m[0x6D] = "ADC";
    m[0x7D] = "ADC"; m[0x79] = "ADC"; m[0x61] = "ADC"; m[0x71] = "ADC";
    m[0xE9] = "SBC"; m[0xE5] = "SBC"; m[0xF5] = "SBC"; m[0xED] = "SBC";
    m[0xFD] = "SBC"; m[0xF9] = "SBC"; m[0xE1] = "SBC"; m[0xF1] = "SBC";
    m[0xC9] = "CMP"; m[0xC5] = "CMP"; m[0xD5] = "CMP"; m[0xCD] = "CMP";
    m[0xDD] = "CMP"; m[0xD9] = "CMP"; m[0xC1] = "CMP"; m[0xD1] = "CMP";
    m[0xE0] = "CPX"; m[0xE4] = "CPX"; m[0xEC] = "CPX";
    m[0xC0] = "CPY"; m[0xC4] = "CPY"; m[0xCC] = "CPY";
    m[0xE6] = "INC"; m[0xF6] = "INC"; m[0xEE] = "INC"; m[0xFE] = "INC";
    m[0xC6] = "DEC"; m[0xD6] = "DEC"; m[0xCE] = "DEC"; m[0xDE] = "DEC";
    m[0xE8] = "INX"; m[0xC8] = "INY"; m[0xCA] = "DEX"; m[0x88] = "DEY";
    m[0x0A] = "ASL"; m[0x06] = "ASL"; m[0x16] = "ASL"; m[0x0E] = "ASL";
    m[0x1E] = "ASL";
    m[0x4A] = "LSR"; m[0x46] = "LSR"; m[0x56] = "LSR"; m[0x4E] = "LSR";
    m[0x5E] = "LSR";
    m[0x2A] = "ROL"; m[0x26] = "ROL"; m[0x36] = "ROL"; m[0x2E] = "ROL";
    m[0x3E] = "ROL";
    m[0x6A] = "ROR"; m[0x66] = "ROR"; m[0x76] = "ROR"; m[0x6E] = "ROR";
    m[0x7E] = "ROR";
    m[0x10] = "BPL"; m[0x30] = "BMI"; m[0x50] = "BVC"; m[0x70] = "BVS";
    m[0x90] = "BCC"; m[0xB0] = "BCS"; m[0xD0] = "BNE"; m[0xF0] = "BEQ";
    m[0x4C] = "JMP"; m[0x6C] = "JMP"; m[0x20] = "JSR"; m[0x60] = "RTS";
    m[0x40] = "RTI"; m[0x00] = "BRK";
    m[0x18] = "CLC"; m[0x38] = "SEC"; m[0x58] = "CLI"; m[0x78] = "SEI";
    m[0xB8] = "CLV"; m[0xD8] = "CLD"; m[0xF8] = "SED"; m[0xEA] = "NOP";
    m[0x1A] = "NOP"; m[0x3A] = "NOP"; m[0x5A] = "NOP"; m[0x7A] = "NOP";
    m[0xDA] = "NOP"; m[0xFA] = "NOP";
    m[0x80] = "NOP"; m[0x82] = "NOP"; m[0x89] = "NOP"; m[0xC2] = "NOP";
    m[0xE2] = "NOP"; m[0x04] = "NOP"; m[0x44] = "NOP"; m[0x64] = "NOP";
    m[0x0C] = "NOP";
    m[0x14] = "NOP"; m[0x34] = "NOP"; m[0x54] = "NOP"; m[0x74] = "NOP";
    m[0xD4] = "NOP"; m[0xF4] = "NOP";
    m[0x1C] = "NOP"; m[0x3C] = "NOP"; m[0x5C] = "NOP"; m[0x7C] = "NOP";
    m[0xDC] = "NOP"; m[0xFC] = "NOP";
    m[0xA7] = "LAX"; m[0xB7] = "LAX"; m[0xAF] = "LAX"; m[0xBF] = "LAX";
    m[0xA3] = "LAX"; m[0xB3] = "LAX";
    m[0x87] = "SAX"; m[0x97] = "SAX"; m[0x8F] = "SAX"; m[0x83] = "SAX";
    m[0xC7] = "DCP"; m[0xD7] = "DCP"; m[0xCF] = "DCP"; m[0xDF] = "DCP";
    m[0xDB] = "DCP"; m[0xD3] = "DCP";
    m[0xE7] = "ISB"; m[0xF7] = "ISB"; m[0xEF] = "ISB"; m[0xFF] = "ISB";
    m[0xFB] = "ISB"; m[0xF3] = "ISB";
    m[0x07] = "SLO"; m[0x17] = "SLO"; m[0x0F] = "SLO"; m[0x1F] = "SLO";
    m[0x1B] = "SLO"; m[0x13] = "SLO";
    m[0x27] = "RLA"; m[0x37] = "RLA"; m[0x2F] = "RLA"; m[0x3F] = "RLA";
    m[0x3B] = "RLA"; m[0x33] = "RLA";
    return m;
}();

/// Text for the instruction PEEKED at pc (no execution, no cycle billing).
inline std::string disassemble_at(Bus& bus, uint16_t pc) {
    const auto rd = [&bus](uint16_t a) { return bus.read(a); };
    const uint8_t op = rd(pc++);
    const Entry& e = kTable[op];
    const char* mn = kMnemonic[op] ? kMnemonic[op] : "???";
    char out[24];
    if (!e.mode) {
        std::snprintf(out, sizeof out, "???");
    } else if (e.mode == mode_imm) {
        std::snprintf(out, sizeof out, "%s #$%02X", mn, rd(pc));
    } else if (e.mode == mode_zp) {
        std::snprintf(out, sizeof out, "%s $%02X", mn, rd(pc));
    } else if (e.mode == mode_zpx) {
        std::snprintf(out, sizeof out, "%s $%02X,X", mn, rd(pc));
    } else if (e.mode == mode_zpy) {
        std::snprintf(out, sizeof out, "%s $%02X,Y", mn, rd(pc));
    } else if (e.mode == mode_abs || e.mode == mode_ind) {
        const uint16_t a = rd(pc) | uint16_t(rd(uint16_t(pc + 1))) << 8;
        if (e.mode == mode_ind)
            std::snprintf(out, sizeof out, "%s ($%04X)", mn, a);
        else
            std::snprintf(out, sizeof out, "%s $%04X", mn, a);
    } else if (e.mode == mode_absx) {
        const uint16_t a = rd(pc) | uint16_t(rd(uint16_t(pc + 1))) << 8;
        std::snprintf(out, sizeof out, "%s $%04X,X", mn, a);
    } else if (e.mode == mode_absy) {
        const uint16_t a = rd(pc) | uint16_t(rd(uint16_t(pc + 1))) << 8;
        std::snprintf(out, sizeof out, "%s $%04X,Y", mn, a);
    } else if (e.mode == mode_izx) {
        std::snprintf(out, sizeof out, "%s ($%02X,X)", mn, rd(pc));
    } else if (e.mode == mode_izy) {
        std::snprintf(out, sizeof out, "%s ($%02X),Y", mn, rd(pc));
    } else if (e.mode == mode_imp &&
               (op == 0x10 || op == 0x30 || op == 0x50 || op == 0x70 ||
                op == 0x90 || op == 0xB0 || op == 0xD0 || op == 0xF0)) {
        // Branches render their ABSOLUTE target (nestest style), not the
        // raw signed offset.
        const uint16_t t = uint16_t(pc + 1 + int8_t(rd(pc)));
        std::snprintf(out, sizeof out, "%s $%04X", mn, t);
    } else if (e.mode == mode_acc) {
        std::snprintf(out, sizeof out, "%s A", mn);
    } else {
        std::snprintf(out, sizeof out, "%s", mn);
    }
    return std::string(out);
}

/// Instruction length in bytes for the trace's operand slots (unknown = 1).
inline uint8_t disasm_len(uint8_t op) {
    const Entry& e = kTable[op];
    if (!e.mode) return 1;
    if (e.mode == mode_imp || e.mode == mode_acc) return 1;
    if (e.mode == mode_imm || e.mode == mode_zp || e.mode == mode_zpx ||
        e.mode == mode_zpy || e.mode == mode_izx || e.mode == mode_izy)
        return 2;
    return 3;  // abs / absx / absy / ind
}

/// Capture everything a log line needs BEFORE executing the instruction.
inline TraceRow peek_trace(Bus& bus, uint16_t pc) {
    TraceRow r;
    r.pc = pc;
    r.op = bus.read(pc);
    const uint8_t len = disasm_len(r.op);
    r.b1 = len >= 2 ? bus.read(uint16_t(pc + 1)) : uint8_t(0);
    r.b2 = len >= 3 ? bus.read(uint16_t(pc + 2)) : uint8_t(0);
    r.text = disassemble_at(bus, pc);
    return r;
}
//@LABS-BEGIN 1
//@LABS-SOLUTION
/// Two hex digits plus trailing space, or three blanks for an empty slot.
inline std::string hex_slot(bool present, uint8_t v) {
    char s[4];
    if (!present) return std::string("   ");
    std::snprintf(s, sizeof s, "%02X ", v);
    return std::string(s);
}

/// Assemble the finished log line: pre-step TraceRow + post-step register
/// and cycle state (see the format block above).
inline std::string trace_line(const Cpu& c, const TraceRow& r) {
    char head[8];
    std::snprintf(head, sizeof head, "%04X  ", r.pc);
    std::string line = head;
    line += hex_slot(true, r.op);
    const uint8_t len = disasm_len(r.op);
    line += hex_slot(len >= 2, r.b1);
    line += hex_slot(len >= 3, r.b2);
    char body[80];
    std::snprintf(body, sizeof body, " %-10s A:%02X X:%02X Y:%02X P:%02X "
                  "SP:%02X CYC:%llu",
                  r.text.c_str(), c.a, c.x, c.y, c.p, c.sp,
                  (unsigned long long)c.cycles);
    line += body;
    return line;
}
//@LABS-STUB
// TODO(1): format "%04X  " + opcode slot + operand slots ("XX " each,
// blank "   " slots for absent operands) + " " + text left-justified in 10
// columns + "A:%02X X:%02X Y:%02X P:%02X SP:%02X CYC:%llu" from the
// POST-step Cpu. hex_slot(present, value) builds one "XX "/"   " slot.
inline std::string hex_slot(bool, uint8_t) {
    return std::string("   ");  // TODO(1)
}

inline std::string trace_line(const Cpu&, const TraceRow&) {
    return std::string("");  // TODO(1)
}
//@LABS-END


}  // namespace nes6502