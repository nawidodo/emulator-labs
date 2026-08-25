#pragma once
#include <cstdint>

#include "../02_opcode_meta/opcode_meta.hpp"
#include "../03_ld_alu/bus.hpp"

namespace gbdbg {

enum Flag : uint8_t {
    FLAG_C = 1u << 4,
    FLAG_H = 1u << 5,
    FLAG_N = 1u << 6,
    FLAG_Z = 1u << 7,
};

// Debug variant of the Chapter 10.3 CPU: same instruction coverage, but the
// skeleton carries three seeded bugs (see DEBUGGING.md). The class layout
// differs slightly from gb::Cpu on purpose: arithmetic and logic ALU halves
// are split into arith8()/logic8() so each bug sits in exactly one block.
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
    bool trap{false};
    uint64_t cyc{0};

    gb::Bus* bus{nullptr};

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
    void set_bc(uint16_t v) { b = uint8_t(v >> 8); c = uint8_t(v); }
    void set_de(uint16_t v) { d = uint8_t(v >> 8); e = uint8_t(v); }
    void set_hl(uint16_t v) { h = uint8_t(v >> 8); l = uint8_t(v); }

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

    // idx: 0=nz 1=z 2=nc 3=c
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
    // Arithmetic half of the ALU: op 0=ADD 1=ADC 2=SUB 3=SBC.
    // `carry_in` folds F.C in for ADC/SBC. Never writes A; caller decides.
    uint8_t arith8(int op, uint8_t lhs, uint8_t rhs, bool carry_in) {
        const unsigned ci = (carry_in && flag_c()) ? 1u : 0u;
        unsigned result;
        if (op < 2) {  // ADD / ADC
            result = lhs + rhs + (op == 1 ? ci : 0);
            set_n(false);
            set_h(((lhs & 0xF) + (rhs & 0xF) + (op == 1 ? ci : 0)) > 0xF);
            set_c(result > 0xFF);
        } else {       // SUB / SBC
            result = lhs - rhs - (op == 3 ? ci : 0);
            set_n(true);
            set_h((lhs & 0xF) < ((rhs & 0xF) + (op == 3 ? ci : 0)));
            set_c(lhs < rhs + (op == 3 ? ci : 0));
        }
        result &= 0xFF;
        set_z(result == 0);
        return static_cast<uint8_t>(result);
    }
    //@LABS-STUB
    // TODO(1): find the bug in this ALU arithmetic half.
    uint8_t arith8(int op, uint8_t lhs, uint8_t rhs, bool carry_in) {
        const unsigned ci = (carry_in && flag_c()) ? 1u : 0u;
        unsigned result;
        if (op < 2) {
            result = lhs + rhs;
            set_n(false);
            set_h(((lhs & 0xF) + (rhs & 0xF)) > 0xF);
            set_c(result > 0xFF);
        } else {
            result = lhs - rhs - (op == 3 ? ci : 0);
            set_n(true);
            set_h((lhs & 0xF) < ((rhs & 0xF) + (op == 3 ? ci : 0)));
            set_c(lhs < rhs + (op == 3 ? ci : 0));
        }
        result &= 0xFF;
        set_z(result == 0);
        return static_cast<uint8_t>(result);
    }
    //@LABS-END

    //@LABS-BEGIN 2
    //@LABS-SOLUTION
    // Logic half of the ALU: op 4=AND 5=XOR 6=OR 7=CP.
    // CP compares only -- the result must NEVER reach A (caller skips the
    // write for op 7, so logic8 itself must not write either).
    uint8_t logic8(int op, uint8_t lhs, uint8_t rhs) {
        unsigned result;
        switch (op) {
            case 4:
                result = lhs & rhs;
                set_z(result == 0);
                set_n(false);
                set_h(true);
                set_c(false);
                break;
            case 5:
                result = lhs ^ rhs;
                set_z(result == 0);
                set_n(false);
                set_h(false);
                set_c(false);
                break;
            case 6:
                result = lhs | rhs;
                set_z(result == 0);
                set_n(false);
                set_h(false);
                set_c(false);
                break;
            default:  // CP: SUB semantics, never writes A
                result = lhs - rhs;
                set_z(result == 0);
                set_n(true);
                set_h((lhs & 0xF) < (rhs & 0xF));
                set_c(lhs < rhs);
                result &= 0xFF;
                return static_cast<uint8_t>(result);
        }
        return static_cast<uint8_t>(result);
    }
    //@LABS-STUB
    // TODO(2): find the bug in this ALU logic half.
    uint8_t logic8(int op, uint8_t lhs, uint8_t rhs) {
        unsigned result;
        switch (op) {
            case 4:
                result = lhs & rhs;
                set_z(result == 0);
                set_n(false);
                set_h(true);
                set_c(false);
                break;
            case 5:
                result = lhs ^ rhs;
                set_z(result == 0);
                set_n(false);
                set_h(false);
                set_c(false);
                break;
            case 6:
                result = lhs | rhs;
                set_z(result == 0);
                set_n(false);
                set_h(false);
                set_c(false);
                break;
            default:
                result = lhs - rhs;
                set_z(result == 0);
                set_n(true);
                set_h((lhs & 0xF) < (rhs & 0xF));
                set_c(lhs < rhs);
                result &= 0xFF;
                a = static_cast<uint8_t>(result);
                return a;
        }
        return static_cast<uint8_t>(result);
    }
    //@LABS-END

    uint8_t fetch8() {
        const uint8_t v = bus->read(pc);
        pc = static_cast<uint16_t>(pc + 1);
        return v;
    }

    uint16_t fetch16() {
        const uint8_t lo = fetch8();
        return static_cast<uint16_t>(lo | fetch8() << 8);
    }

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

    void add_hl(uint16_t rhs) {
        const uint16_t lhs = hl();
        set_h(((lhs & 0xFFF) + (rhs & 0xFFF)) > 0xFFF);
        set_c(lhs + rhs > 0xFFFF);
        set_n(false);
        set_hl(static_cast<uint16_t>(lhs + rhs));
    }

    // JP/JR conditional dispatch shared by both families. `is_jr` selects
    // the 1-byte-relative form.
    void exec_cond_jump(const gb::Instruction& info, int y, bool is_jr,
                        int& cycles) {
//@LABS-BEGIN 3
//@LABS-SOLUTION
        const bool taken = condition(y & 3);
        if (is_jr) {
            const auto offset = static_cast<int8_t>(fetch8());
            if (taken) {
                pc = static_cast<uint16_t>(pc + offset);
                cycles += info.cycles_alt;  // taken JR pays the extra M-cycle
            }
        } else {
            const uint16_t target = fetch16();
            if (taken) {
                pc = target;
                cycles += info.cycles_alt;
            }
        }
//@LABS-STUB
        // TODO(3): something is off with conditional-jump timing.
        const bool taken = condition(y & 3);
        if (is_jr) {
            const auto offset = static_cast<int8_t>(fetch8());
            if (taken) {
                pc = static_cast<uint16_t>(pc + offset);
            }
            cycles += info.cycles_alt;  // charged even when NOT taken?
        } else {
            const uint16_t target = fetch16();
            if (taken) {
                pc = target;
                cycles += info.cycles_alt;
            }
        }
//@LABS-END
    }

    bool exec(uint8_t op, int& cycles) {
        const gb::Instruction& info = gb::opcode_info(op);
        cycles = info.cycles;
        const int x = op >> 6;
        const int y = (op >> 3) & 7;
        const int z = op & 7;

        if (x == 1) {
            if (op == 0x76) {
                halted = true;
                return true;
            }
            set_r(y, r(z));
            return true;
        }
        if (x == 2) {
            uint8_t res;
            if (y <= 3) {
                res = arith8(y, a, r(z), y == 1 || y == 3);
            } else {
                res = logic8(y, a, r(z));
            }
            if (y != 7) a = res;
            return true;
        }

        const int p = (op >> 4) & 3;
        switch (op) {
            case 0x00: return true;
            case 0x18:
            case 0x20: case 0x28: case 0x30: case 0x38:
                exec_cond_jump(info, y, true, cycles);
                return true;
            case 0xC2: case 0xCA: case 0xD2: case 0xDA:
                exec_cond_jump(info, y, false, cycles);
                return true;
            case 0xC3: pc = fetch16(); return true;
            case 0x01: case 0x11: case 0x21: case 0x31:
                set_rp(p, fetch16()); return true;
            case 0x09: case 0x19: case 0x29: case 0x39:
                add_hl(rp(p)); return true;
            case 0x03: case 0x13: case 0x23: case 0x33:
                set_rp(p, static_cast<uint16_t>(rp(p) + 1)); return true;
            case 0x22: bus->write(hl(), a); set_hl(static_cast<uint16_t>(hl() + 1)); return true;
            case 0x2A: a = bus->read(hl()); set_hl(static_cast<uint16_t>(hl() + 1)); return true;
            case 0x06: case 0x0E: case 0x16: case 0x1E:
            case 0x26: case 0x2E: case 0x3E:
                set_r(y, fetch8()); return true;
            case 0xC6: case 0xCE: case 0xD6: case 0xDE:
            case 0xE6: case 0xEE: case 0xF6: case 0xFE: {
                const uint8_t operand = fetch8();
                const uint8_t res = (y <= 3) ? arith8(y, a, operand, y == 1 || y == 3)
                                             : logic8(y, a, operand);
                if (y != 7) a = res;
                return true;
            }
            default:
                return false;
        }
    }

    int step() {
        if (trap) return 0;
        if (halted) return 4;
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
};

}  // namespace gbdbg
