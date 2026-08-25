#pragma once
#include <cstdint>

#include "core.hpp"

namespace gb {

// Chapter 11.1: DAA, the base-page rotates, CPL/SCF/CCF, and the full CB
// page. Installed as one hook covering 0x07/0x0F/0x17/0x1F/0x27/0x2F/
// 0x37/0x3F and everything behind the 0xCB prefix.

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Exact DAA. Inputs are A plus the current N/H/C; the operation that ran
// before DAA decides the adjustment direction. The two tests use the
// ORIGINAL value of A (adding 0x60 never disturbs the low nibble, so the
// order below matches hardware).
inline void daa(Cpu& cpu) {
    uint8_t a = cpu.a;
    if (!cpu.flag_n()) {
        if (cpu.flag_c() || a > 0x99) {
            a = static_cast<uint8_t>(a + 0x60);
            cpu.set_c(true);
        }
        if (cpu.flag_h() || (a & 0x0F) > 0x09) {
            a = static_cast<uint8_t>(a + 0x06);
        }
    } else {
        if (cpu.flag_c()) a = static_cast<uint8_t>(a - 0x60);
        if (cpu.flag_h()) a = static_cast<uint8_t>(a - 0x06);
    }
    cpu.a = a;
    cpu.set_z(a == 0);
    cpu.set_h(false);
}
//@LABS-STUB
// TODO(1): implement exact DAA (both the after-addition and
// after-subtraction paths, including the half-carry adjustments).
inline void daa(Cpu& cpu) {
    (void)cpu;
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Base-page rotates through the carry and the complement/carry-flag ops.
// All clear N and H; rotates set Z from the result and C from the shifted-
// out bit.
inline uint8_t rotate_left_carry(Cpu& cpu, uint8_t v) {  // RLCA / RLC
    const uint8_t res = static_cast<uint8_t>((v << 1) | (v >> 7));
    cpu.set_c((v & 0x80) != 0);
    return res;
}

inline uint8_t rotate_right_carry(Cpu& cpu, uint8_t v) {  // RRCA / RRC
    const uint8_t res = static_cast<uint8_t>((v >> 1) | (v << 7));
    cpu.set_c((v & 0x01) != 0);
    return res;
}

inline uint8_t rotate_left(Cpu& cpu, uint8_t v) {  // RLA / RL
    const uint8_t res =
        static_cast<uint8_t>((v << 1) | (cpu.flag_c() ? 1 : 0));
    cpu.set_c((v & 0x80) != 0);
    return res;
}

inline uint8_t rotate_right(Cpu& cpu, uint8_t v) {  // RRA / RR
    const uint8_t res =
        static_cast<uint8_t>((v >> 1) | (cpu.flag_c() ? 0x80 : 0));
    cpu.set_c((v & 0x01) != 0);
    return res;
}

inline void cpl(Cpu& cpu) {  // A = ~A; N=1 H=1, Z/C kept
    cpu.a = static_cast<uint8_t>(~cpu.a);
    cpu.set_n(true);
    cpu.set_h(true);
}

inline void scf(Cpu& cpu) {  // C=1, N=0 H=0, Z kept
    cpu.set_c(true);
    cpu.set_n(false);
    cpu.set_h(false);
}

inline void ccf(Cpu& cpu) {  // C=!C, N=0 H=0, Z kept
    cpu.set_c(!cpu.flag_c());
    cpu.set_n(false);
    cpu.set_h(false);
}
//@LABS-STUB
// TODO(2): implement the rotate-through-carry helpers and CPL/SCF/CCF
// flag contracts.
inline uint8_t rotate_left_carry(Cpu& cpu, uint8_t v) {
    (void)cpu; (void)v;
    return 0;  // wrong on purpose
}
inline uint8_t rotate_right_carry(Cpu& cpu, uint8_t v) {
    (void)cpu; (void)v;
    return 0;  // wrong on purpose
}
inline uint8_t rotate_left(Cpu& cpu, uint8_t v) {
    (void)cpu; (void)v;
    return 0;  // wrong on purpose
}
inline uint8_t rotate_right(Cpu& cpu, uint8_t v) {
    (void)cpu; (void)v;
    return 0;  // wrong on purpose
}
inline void cpl(Cpu& cpu) { (void)cpu; }   // TODO(2)
inline void scf(Cpu& cpu) { (void)cpu; }   // TODO(2)
inline void ccf(Cpu& cpu) { (void)cpu; }   // TODO(2)
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// Shifts and SWAP (CB page only).
inline uint8_t shift_arith_left(Cpu& cpu, uint8_t v) {  // SLA
    cpu.set_c((v & 0x80) != 0);
    return static_cast<uint8_t>(v << 1);
}

inline uint8_t shift_arith_right(Cpu& cpu, uint8_t v) {  // SRA (sign keep)
    cpu.set_c((v & 0x01) != 0);
    return static_cast<uint8_t>((v >> 1) | (v & 0x80));
}

inline uint8_t shift_logic_right(Cpu& cpu, uint8_t v) {  // SRL
    cpu.set_c((v & 0x01) != 0);
    return static_cast<uint8_t>(v >> 1);
}

inline uint8_t swap_nibbles(Cpu& cpu, uint8_t v) {  // SWAP
    cpu.set_c(false);
    return static_cast<uint8_t>((v << 4) | (v >> 4));
}

// Shared tail for every CB result: Z from result, N/H cleared.
inline void finish_cb(Cpu& cpu, uint8_t res) {
    cpu.set_z(res == 0);
    cpu.set_n(false);
    cpu.set_h(false);
}
//@LABS-STUB
// TODO(3): implement SLA/SRA/SRL/SWAP and the shared CB flag tail
// (Z from result, N=0, H=0).
inline uint8_t shift_arith_left(Cpu& cpu, uint8_t v) {
    (void)cpu; (void)v; return 0;
}
inline uint8_t shift_arith_right(Cpu& cpu, uint8_t v) {
    (void)cpu; (void)v; return 0;
}
inline uint8_t shift_logic_right(Cpu& cpu, uint8_t v) {
    (void)cpu; (void)v; return 0;
}
inline uint8_t swap_nibbles(Cpu& cpu, uint8_t v) {
    (void)cpu; (void)v; return 0;
}
inline void finish_cb(Cpu& cpu, uint8_t res) {
    (void)cpu; (void)res;
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// CB-page executor. Layout: xxxyyzzz with x selecting the operation group,
// y the bit/op index, z the register ((6) routes through HL).
inline bool cb_exec(Cpu& cpu, uint8_t op, int& cycles) {
    const Instruction& info = cb_info(op);  // bytes=2 covers the prefix too
    cycles = info.cycles;
    const int x = op >> 6;
    const int y = (op >> 3) & 7;
    const int z = op & 7;

    uint8_t v = cpu.r(z);
    uint8_t res = v;
    switch (x) {
        case 0:  // rotates/shifts, y picks the flavor
            switch (y) {
                case 0: res = rotate_left_carry(cpu, v); break;
                case 1: res = rotate_right_carry(cpu, v); break;
                case 2: res = rotate_left(cpu, v); break;
                case 3: res = rotate_right(cpu, v); break;
                case 4: res = shift_arith_left(cpu, v); break;
                case 5: res = shift_arith_right(cpu, v); break;
                case 6: res = swap_nibbles(cpu, v); break;
                default: res = shift_logic_right(cpu, v); break;
            }
            finish_cb(cpu, res);
            cpu.set_r(z, res);
            return true;
        case 1:  // BIT y,r: read-only, Z = bit clear; C untouched!
            cpu.set_z((v & (1u << y)) == 0);
            cpu.set_n(false);
            cpu.set_h(true);
            return true;
        case 2:  // RES y,r
            cpu.set_r(z, static_cast<uint8_t>(v & ~(1u << y)));
            return true;
        default:  // SET y,r
            cpu.set_r(z, static_cast<uint8_t>(v | (1u << y)));
            return true;
    }
}
//@LABS-STUB
// TODO(4): implement the CB-page dispatcher (rotates/shifts/SWAP write the
// register and finish_cb; BIT only sets flags -- N=0 H=1, C untouched;
// RES/SET modify without touching flags). Cycle cost comes from cb_info().
inline bool cb_exec(Cpu& cpu, uint8_t op, int& cycles) {
    (void)cpu; (void)op; (void)cycles;
    return false;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 5
//@LABS-SOLUTION
// Hook covering the misc base rows plus the whole CB page.
inline bool daa_exec(Cpu& cpu, uint8_t op, int& cycles) {
    switch (op) {
        case 0x07: {  // RLCA: Z=0 always on this form!
            cpu.a = rotate_left_carry(cpu, cpu.a);
            cpu.set_z(false);
            cpu.set_n(false);
            cpu.set_h(false);
            cycles = 4;
            return true;
        }
        case 0x0F: {  // RRCA
            cpu.a = rotate_right_carry(cpu, cpu.a);
            cpu.set_z(false);
            cpu.set_n(false);
            cpu.set_h(false);
            cycles = 4;
            return true;
        }
        case 0x17: {  // RLA
            cpu.a = rotate_left(cpu, cpu.a);
            cpu.set_z(false);
            cpu.set_n(false);
            cpu.set_h(false);
            cycles = 4;
            return true;
        }
        case 0x1F: {  // RRA
            cpu.a = rotate_right(cpu, cpu.a);
            cpu.set_z(false);
            cpu.set_n(false);
            cpu.set_h(false);
            cycles = 4;
            return true;
        }
        case 0x27: daa(cpu); cycles = 4; return true;
        case 0x2F: cpl(cpu); cycles = 4; return true;
        case 0x37: scf(cpu); cycles = 4; return true;
        case 0x3F: ccf(cpu); cycles = 4; return true;
        default: break;
    }
    if (op == 0xCB) {
        const uint8_t sub = cpu.fetch8();
        return cb_exec(cpu, sub, cycles);
    }
    return false;
}
//@LABS-STUB
// TODO(5): wire the misc base rows and the 0xCB prefix dispatch into a
// hook. Remember: the x0 rotate forms force Z=0 (unlike their CB twins).
inline bool daa_exec(Cpu& cpu, uint8_t op, int& cycles) {
    (void)cpu; (void)op; (void)cycles;
    return false;  // wrong on purpose
}
//@LABS-END

namespace detail {
inline bool daa_hook_fn(void*, Cpu& cpu, uint8_t op, int& cycles) {
    return daa_exec(cpu, op, cycles);
}
}  // namespace detail

// Convenience: install as the first hook of a chain.
inline void install_daa_hook(Cpu& cpu) {
    static Cpu::Hook hook{&detail::daa_hook_fn, nullptr, nullptr};
    hook.next = cpu.hooks;
    cpu.hooks = &hook;
}

}  // namespace gb
