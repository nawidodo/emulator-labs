#pragma once
#include <cstdint>

#include "../02_opcode_meta/opcode_meta.hpp"
#include "bus.hpp"

namespace gb {

// Flag masks inside F (low nibble always 0).
enum Flag : uint8_t {
    FLAG_C = 1u << 4,
    FLAG_H = 1u << 5,
    FLAG_N = 1u << 6,
    FLAG_Z = 1u << 7,
};


struct Cpu {
    uint8_t a{0x01};
    uint8_t f{0xB0};
    uint8_t b{0x00};
    uint8_t c{0x13};
    uint8_t d{0x00};
    uint8_t e{0xD8};
    uint8_t h{0x01};
    uint8_t l{0x4D};
    uint16_t sp{0xFFFE};
    uint16_t pc{0x0100};
    bool halted{false};
    bool trap{false};     // set on unimplemented opcode; runner stops
    uint64_t cyc{0};      // total T-cycles executed

    Bus* bus{nullptr};

    // ---- register plumbing (plain text: exercised in 01_cpu_state) ----
    bool flag_z() const { return (f & FLAG_Z) != 0; }
    bool flag_n() const { return (f & FLAG_N) != 0; }
    bool flag_h() const { return (f & FLAG_H) != 0; }
    bool flag_c() const { return (f & FLAG_C) != 0; }
    void set_z(bool v) { f = v ? uint8_t(f | FLAG_Z) : uint8_t(f & ~FLAG_Z); }
    void set_n(bool v) { f = v ? uint8_t(f | FLAG_N) : uint8_t(f & ~FLAG_N); }
    void set_h(bool v) { f = v ? uint8_t(f | FLAG_H) : uint8_t(f & ~FLAG_H); }
    void set_c(bool v) { f = v ? uint8_t(f | FLAG_C) : uint8_t(f & ~FLAG_C); }

    uint16_t bc() const { return uint16_t(b) << 8 | c; }
    uint16_t de() const { return uint16_t(d) << 8 | e; }
    uint16_t hl() const { return uint16_t(h) << 8 | l; }
    uint16_t af() const { return uint16_t(a) << 8 | (f & 0xF0); }
    void set_bc(uint16_t v) { b = uint8_t(v >> 8); c = uint8_t(v); }
    void set_de(uint16_t v) { d = uint8_t(v >> 8); e = uint8_t(v); }
    void set_hl(uint16_t v) { h = uint8_t(v >> 8); l = uint8_t(v); }

    // Register-file access by encoding index 0..7 ((6) routes through the
    // bus because index 6 *is* (HL)).
    uint8_t r(int idx) const {
        switch (idx) {
            case 0: return b;
            case 1: return c;
            case 2: return d;
            case 3: return e;
            case 4: return h;
            case 5: return l;
            case 7: return a;
            default: return bus->read(hl());
        }
    }

    void set_r(int idx, uint8_t v) {
        switch (idx) {
            case 0: b = v; break;
            case 1: c = v; break;
            case 2: d = v; break;
            case 3: e = v; break;
            case 4: h = v; break;
            case 5: l = v; break;
            case 7: a = v; break;
            default: bus->write(hl(), v); break;
        }
    }

    uint16_t rp(int idx) const {
        switch (idx) {
            case 0: return bc();
            case 1: return de();
            case 2: return hl();
            default: return sp;
        }
    }

    void set_rp(int idx, uint16_t v) {
        switch (idx) {
            case 0: set_bc(v); break;
            case 1: set_de(v); break;
            case 2: set_hl(v); break;
            default: sp = v; break;
        }
    }

    bool condition(int idx) const {
        switch (idx) {
            case 0: return !flag_z();
            case 1: return flag_z();
            case 2: return !flag_c();
            default: return flag_c();
        }
    }

    //@LABS-BEGIN 1
    //@LABS-SOLUTION
    // Core 8-bit ALU. Returns the result and writes ALL four flags according
    // to the operation class:
    //   arithmetic: Z=result==0, N=subtraction, H=nibble carry/borrow,
    //               C=byte carry/borrow
    //   logic:      Z=result==0, everything else pinned by the op
    // `with_carry` folds F.C in (ADC/SBC); CP behaves like SUB without
    // writing the destination.
    uint8_t alu8(int op, uint8_t lhs, uint8_t rhs, bool with_carry,
                 bool write_back) {
        const unsigned carry_in = (with_carry && flag_c()) ? 1u : 0u;
        unsigned result = 0;
        switch (op) {
            case 0:  // ADD
                result = lhs + rhs;
                set_n(false);
                set_h(((lhs & 0xF) + (rhs & 0xF)) > 0xF);
                set_c(result > 0xFF);
                break;
            case 1:  // ADC
                result = lhs + rhs + carry_in;
                set_n(false);
                set_h(((lhs & 0xF) + (rhs & 0xF) + carry_in) > 0xF);
                set_c(result > 0xFF);
                break;
            case 2:  // SUB
            case 7:  // CP
                result = lhs - rhs;
                set_n(true);
                set_h(static_cast<int>(lhs & 0xF) - static_cast<int>(rhs & 0xF) < 0);
                set_c(lhs < rhs);
                break;
            case 3:  // SBC
                result = lhs - rhs - carry_in;
                set_n(true);
                set_h((lhs & 0xF) < (rhs & 0xF) + carry_in);
                set_c(lhs < rhs + carry_in);
                break;
            case 4:  // AND
                result = lhs & rhs;
                set_z(result == 0);
                set_n(false);
                set_h(true);
                set_c(false);
                break;
            case 5:  // XOR
                result = lhs ^ rhs;
                set_z(result == 0);
                set_n(false);
                set_h(false);
                set_c(false);
                break;
            default:  // OR
                result = lhs | rhs;
                set_z(result == 0);
                set_n(false);
                set_h(false);
                set_c(false);
                break;
        }
        result &= 0xFF;
        if (write_back && op != 7) a = static_cast<uint8_t>(result);
        set_z(static_cast<uint8_t>(result) == 0);
        if (op == 7) return a;  // CP never writes
        return static_cast<uint8_t>(result);
    }
    //@LABS-STUB
    // TODO(1): implement the 8-bit ALU (ADD ADC SUB SBC AND XOR OR CP).
    // Set Z/N/H/C per operation class; CP must not write A.
    uint8_t alu8(int op, uint8_t lhs, uint8_t rhs, bool with_carry,
                 bool write_back) {
        (void)op; (void)lhs; (void)rhs;
        (void)with_carry; (void)write_back;
        return 0;  // wrong on purpose
    }
    //@LABS-END

    //@LABS-BEGIN 2
    //@LABS-SOLUTION
    // INC/DEC preserve C, set N appropriately, H on nibble overflow/borrow.
    void inc8(uint8_t& v) {
        set_h((v & 0xF) == 0xF);
        set_n(false);
        ++v;
        set_z(v == 0);
    }

    void dec8(uint8_t& v) {
        set_h((v & 0xF) == 0);
        set_n(true);
        --v;
        set_z(v == 0);
    }
    //@LABS-STUB
    // TODO(2): implement INC/DEC flag semantics (Z/N/H updated, C kept).
    void inc8(uint8_t& v) { (void)v; }  // TODO(2)
    void dec8(uint8_t& v) { (void)v; }  // TODO(2)
    //@LABS-END

    //@LABS-BEGIN 3
    //@LABS-SOLUTION
    // 16-bit add into HL: H carries out of bit 11, C out of bit 15, N=0,
    // Z untouched.
    void add_hl(uint16_t rhs) {
        const uint16_t lhs = hl();
        set_h(((lhs & 0xFFF) + (rhs & 0xFFF)) > 0xFFF);
        set_c(lhs + rhs > 0xFFFF);
        set_n(false);
        set_hl(static_cast<uint16_t>(lhs + rhs));
    }
    //@LABS-STUB
    // TODO(3): implement ADD HL,rr flag behavior (N=0, H bit-11 carry,
    // C bit-15 carry, Z preserved).
    void add_hl(uint16_t rhs) { (void)rhs; }  // TODO(3)
    //@LABS-END

    // Extension point: later chapters / coding tests plug extra opcode
    // families in without touching this decoder. Return true when handled.
    bool (*extra_exec)(Cpu&, uint8_t op, int& cycles) = nullptr;

    uint8_t fetch8() {
        const uint8_t v = bus->read(pc);
        pc = static_cast<uint16_t>(pc + 1);
        return v;
    }

    uint16_t fetch16() {
        const uint8_t lo = fetch8();
        return static_cast<uint16_t>(lo | fetch8() << 8);
    }

    //@LABS-BEGIN 4
    //@LABS-SOLUTION
    // Decode + execute one opcode (already fetched, PC past it). Returns
    // false for opcodes this chapter does not implement (caller traps).
    // Cycle counts come straight from the 02_opcode_meta tables; conditional
    // instructions add cycles_alt only when the branch is taken.
    bool exec(uint8_t op, int& cycles) {
        if (extra_exec && extra_exec(*this, op, cycles)) return true;

        const Instruction& info = opcode_info(op);
        cycles = info.cycles;
        const int x = op >> 6;
        const int y = (op >> 3) & 7;
        const int z = op & 7;

        if (x == 1) {  // LD r[y],r[z]; y==z==6 (0x76) is HALT
            if (op == 0x76) {
                halted = true;
                return true;
            }
            set_r(y, r(z));
            return true;
        }
        if (x == 2) {  // ALU A,r[z]
            const bool subtractive = (y == 2 || y == 3 || y == 7);
            alu8(y, a, r(z), y == 1 || y == 3, !subtractive);
            return true;
        }

        // Pair ops live in bits 4-5 of x=0 opcodes (NOT the z field).
        const int p = (op >> 4) & 3;
        switch (op) {
            case 0x00:  // nop
                return true;

            case 0x76:  // halt (simple version; wake semantics arrive in ch11)
                halted = true;
                return true;

            // 16-bit immediate loads
            case 0x01: case 0x11: case 0x21: case 0x31:
                set_rp(p, fetch16());
                return true;

            // INC/DEC rr
            case 0x03: case 0x13: case 0x23: case 0x33:
                set_rp(p, static_cast<uint16_t>(rp(p) + 1));
                return true;
            case 0x0B: case 0x1B: case 0x2B: case 0x3B:
                set_rp(p, static_cast<uint16_t>(rp(p) - 1));
                return true;

            // ADD HL,rr
            case 0x09: case 0x19: case 0x29: case 0x39:
                add_hl(rp(p));
                return true;

            // LD (rr),A and LDI/LDD forms
            case 0x02: bus->write(bc(), a); return true;
            case 0x12: bus->write(de(), a); return true;
            case 0x22: bus->write(hl(), a); set_hl(static_cast<uint16_t>(hl() + 1)); return true;
            case 0x32: bus->write(hl(), a); set_hl(static_cast<uint16_t>(hl() - 1)); return true;

            // LD A,(rr) and LDI/LDD forms
            case 0x0A: a = bus->read(bc()); return true;
            case 0x1A: a = bus->read(de()); return true;
            case 0x2A: a = bus->read(hl()); set_hl(static_cast<uint16_t>(hl() + 1)); return true;
            case 0x3A: a = bus->read(hl()); set_hl(static_cast<uint16_t>(hl() - 1)); return true;

            // LD SP,HL / JP HL
            case 0xF9: sp = hl(); return true;
            case 0xE9: pc = hl(); return true;

            // JP nn / JP cc,nn
            case 0xC3: {
                const uint16_t target = fetch16();
                pc = target;
                return true;
            }
            case 0xC2: case 0xCA: case 0xD2: case 0xDA: {
                const uint16_t target = fetch16();
                if (condition(y & 3)) {
                    pc = target;
                    cycles += info.cycles_alt;
                }
                return true;
            }

            // JR e / JR cc,e
            case 0x18: {
                const auto offset = static_cast<int8_t>(fetch8());
                pc = static_cast<uint16_t>(pc + offset);
                return true;
            }
            case 0x20: case 0x28: case 0x30: case 0x38: {
                const auto offset = static_cast<int8_t>(fetch8());
                if (condition(y & 3)) {
                    pc = static_cast<uint16_t>(pc + offset);
                    cycles += info.cycles_alt;
                }
                return true;
            }

            // LD (nn),A / LD A,(nn)
            case 0xEA: {
                const uint16_t target = fetch16();
                bus->write(target, a);
                return true;
            }
            case 0xFA: {
                const uint16_t target = fetch16();
                a = bus->read(target);
                return true;
            }

            // ALU A,n immediate forms (same flag contract as the x==2 block)
            case 0xC6: case 0xCE: case 0xD6: case 0xDE:
            case 0xE6: case 0xEE: case 0xF6: case 0xFE: {
                const int y = (op >> 3) & 7;
                const uint8_t operand = fetch8();
                alu8(y, a, operand, y == 1 || y == 3, y != 7);
                return true;
            }

            // 8-bit INC/DEC/LD rows (registers and (HL))
            case 0x04: case 0x0C: case 0x14: case 0x1C:
            case 0x24: case 0x2C: case 0x3C: {
                uint8_t v = r(y);
                inc8(v);
                set_r(y, v);
                return true;
            }
            case 0x05: case 0x0D: case 0x15: case 0x1D:
            case 0x25: case 0x2D: case 0x3D: {
                uint8_t v = r(y);
                dec8(v);
                set_r(y, v);
                return true;
            }
            case 0x06: case 0x0E: case 0x16: case 0x1E:
            case 0x26: case 0x2E: case 0x3E:
                set_r(y, fetch8());
                return true;

            case 0x34: {
                uint8_t v = bus->read(hl());
                inc8(v);
                bus->write(hl(), v);
                return true;
            }
            case 0x35: {
                uint8_t v = bus->read(hl());
                dec8(v);
                bus->write(hl(), v);
                return true;
            }
            case 0x36:
                bus->write(hl(), fetch8());
                return true;

            default:
                return false;  // unimplemented: caller decides policy
        }
    }
    //@LABS-STUB
    // TODO(4): implement exec(). Decode via x/y/z fields; take cycle counts
    // from opcode_info(); conditional jumps add cycles_alt when taken.
    // Cover: LD r,r' / LD r,n / LD rr,nn / indirect A loads / INC DEC
    // (8-bit + pairs) / ADD HL,rr / ALU A,x / JP / JR / HALT.
    bool exec(uint8_t op, int& cycles) {
        (void)op;
        cycles = 0;
        return false;  // wrong on purpose
    }
    //@LABS-END

    //@LABS-BEGIN 5
    //@LABS-SOLUTION
    // One instruction boundary: fetch, execute, account cycles. Traps roll
    // PC back so a debugger sees the offending instruction.
    // Returns T-cycles consumed (0 after a trap or while halted).
    int step() {
        if (trap) return 0;
        if (halted) return 4;  // burn time; interrupt wake arrives in ch13
        const uint16_t instr_pc = pc;
        const uint8_t op = fetch8();
        int used = 0;
        if (!exec(op, used)) {
            trap = true;
            pc = instr_pc;
            return 0;
        }
        cyc += static_cast<uint64_t>(used);
        return used;
    }
    //@LABS-STUB
    // TODO(5): implement step(): fetch opcode at PC, run exec(), accumulate
    // cycles, trap on unimplemented opcodes (restore PC first).
    int step() {
        // Stub safety: burn 4 cycles and advance PC so run()-style loops
        // always terminate (RED, never hangs).
        cyc += 4;
        pc = static_cast<uint16_t>((pc + 1) & 0xFFFF);
        return 4;  // wrong on purpose
    }
    //@LABS-END
};

}  // namespace gb
