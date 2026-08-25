#include "cpu.hpp"

namespace i8080 {

void Cpu::reset() {
    a = b = c = d = e = h = l = 0;
    sp = 0;
    pc = 0;
    s = z = p = cy = ac = false;
    halted = false;
    cycles = 0;
}

void Cpu::set_szp(uint8_t v) {
    const uint8_t f = szp_bits(v);
    s = (f & FLAG_S) != 0;
    z = (f & FLAG_Z) != 0;
    p = (f & FLAG_P) != 0;
}

namespace {

// Register-field decode shared by every group: 0..7 map to B C D E H L M A,
// where 6 (M) means "memory at HL".
constexpr uint8_t REG_B = 0, REG_C = 1, REG_D = 2, REG_E = 3,
                  REG_H = 4, REG_L = 5, REG_M = 6, REG_A = 7;

}  // namespace

namespace {

void stack_push(Cpu& c, uint16_t value) {
    // The 8080 stack grows downward: pre-decrement SP twice, store high
    // byte at SP-1 and low byte at SP-2.
    c.bus->write(uint16_t(c.sp - 1), uint8_t(value >> 8));
    c.bus->write(uint16_t(c.sp - 2), uint8_t(value));
    c.sp = uint16_t(c.sp - 2);
}

uint16_t stack_pop(Cpu& c) {
//@LABS-BEGIN 3
//@LABS-SOLUTION
    const uint8_t lo = c.bus->read(c.sp);
    const uint8_t hi = c.bus->read(uint16_t(c.sp + 1));
//@LABS-STUB
    // BUG(2): bytes read back-to-front. RET therefore lands on a
    // byte-swapped return address (e.g. returning to 0x0500 instead of
    // 0x0005) — execution wanders into padding and any register dump is
    // garbage from that point on. POP PSW swaps halves the same way.
    const uint8_t lo = c.bus->read(uint16_t(c.sp + 1));
    const uint8_t hi = c.bus->read(c.sp);
//@LABS-END
    c.sp = uint16_t(c.sp + 2);
    return uint16_t(uint16_t(hi) << 8 | lo);
}

}  // namespace

uint64_t Cpu::step() {
    if (halted) return 0;

    const uint16_t start_pc = pc;
    uint8_t opcode = 0;
    uint64_t cost = 0;

    // Fetch: PC always advances by the full instruction length, even when
    // a later stage discards bytes (CMP keeps flags but no result).
    auto fetch8 = [&]() -> uint8_t {
        const uint8_t v = bus ? bus->read(pc) : 0;
        pc = uint16_t(pc + 1);
        return v;
    };
    auto fetch16 = [&]() -> uint16_t {
        const uint8_t lo = fetch8();
        return uint16_t(lo | fetch8() << 8);   // 8080 is little-endian
    };

    // Register-field access; M reads/writes through HL.
    auto get_reg = [&](uint8_t idx) -> uint8_t {
        switch (idx) {
            case REG_B: return b;
            case REG_C: return c;
            case REG_D: return d;
            case REG_E: return e;
            case REG_H: return h;
            case REG_L: return l;
            case REG_M: return bus ? bus->read(hl()) : 0;
            default: return a;
        }
    };
    auto set_reg = [&](uint8_t idx, uint8_t v) {
        switch (idx) {
            case REG_B: b = v; break;
            case REG_C: c = v; break;
            case REG_D: d = v; break;
            case REG_E: e = v; break;
            case REG_H: h = v; break;
            case REG_L: l = v; break;
            case REG_M: if (bus) bus->write(hl(), v); break;
            default: a = v; break;
        }
    };

    // Condition-code decode shared by jumps/calls/returns:
    // 000 NZ | 001 Z | 010 NC | 011 C | 100 PO | 101 PE | 110 P | 111 M
    auto condition = [&](uint8_t cc) -> bool {
        switch (cc) {
            case 0: return !z;
            case 1: return z;
            case 2: return !cy;
            case 3: return cy;
            case 4: return !p;
            case 5: return p;
            case 6: return !s;
            default: return s;
        }
    };

    opcode = fetch8();
    (void)start_pc;

    // MOV r,r' : 01DDDSSS — 5 T-states between registers, 7 when either
    // side touches memory (M). 0x76 is HLT, not MOV M,M.
    if ((opcode & 0xC0) == 0x40 && opcode != 0x76) {
        const uint8_t dst = (opcode >> 3) & 7;
        const uint8_t src = opcode & 7;
        set_reg(dst, get_reg(src));
        cost = (dst == REG_M || src == REG_M) ? 7 : 5;
        cycles += cost;
        return cost;
    }

    // MVI r,data : 00DDD110 — immediate byte follows the opcode.
    if ((opcode & 0xC7) == 0x06) {
        const uint8_t dst = (opcode >> 3) & 7;
        const uint8_t imm = fetch8();
        set_reg(dst, imm);
        cost = (dst == REG_M) ? 10 : 7;
        cycles += cost;
        return cost;
    }

    // LXI rp,d16 : 00RP0001 — rp selects BC/DE/HL/SP. 10 T-states.
    if ((opcode & 0xCF) == 0x01) {
        const uint16_t imm = fetch16();
        switch ((opcode >> 4) & 3) {
            case 0: set_bc(imm); break;
            case 1: set_de(imm); break;
            case 2: set_hl(imm); break;
            default: sp = imm; break;   // chapter 8: SP becomes loadable
        }
        cost = 10;
        cycles += cost;
        return cost;
    }

    // LDA addr / STA addr : direct A <-> memory, 16-bit address operand.
    // Both take 13 T-states.
    if (opcode == 0x3A) {
        a = bus ? bus->read(fetch16()) : 0;
        cost = 13;
        cycles += cost;
        return cost;
    }
    if (opcode == 0x32) {
        const uint16_t addr = fetch16();
        if (bus) bus->write(addr, a);
        cost = 13;
        cycles += cost;
        return cost;
    }

    // INR r : 00DDD100 and DCR r : 00DDD101. CY is PRESERVED; AC follows
    // the nibble rules (set when low nibble overflows/borrows); S/Z/P from
    // the new value. 5 T-states for registers, 10 through memory.
    if ((opcode & 0xC7) == 0x04 || (opcode & 0xC7) == 0x05) {
        const bool inc = (opcode & 0x07) == 0x04;
        const uint8_t dst = (opcode >> 3) & 7;
        const uint8_t old_v = get_reg(dst);
        const uint8_t new_v =
            inc ? uint8_t(old_v + 1) : uint8_t(old_v - 1);
        set_reg(dst, new_v);
        ac = inc ? aux_carry_add(old_v, 0x01, false)
                 : aux_carry_sub(old_v, 0x01, false);
        set_szp(new_v);
        cost = (dst == REG_M) ? 10 : 5;
        cycles += cost;
        return cost;
    }

    // ALU group, register operand: 10OOOSSSS with SSS as the register field.
    // Group code (ooo): 0=ADD 1=ADC 2=SUB 3=SBB 4=ANA 5=XRA 6=ORA 7=CMP.
    // Immediate forms use the same group codes in 11OOO110.
    if ((opcode & 0xC0) == 0x80 ||
        ((opcode & 0xC7) == 0xC6)) {
        const uint8_t group = (opcode >> 3) & 7;
        const bool immediate = (opcode & 0xC7) == 0xC6;
        const uint8_t v = immediate ? fetch8()
                                    : get_reg(opcode & 7);
        AluResult r{};
        switch (group) {
            case 0: r = alu_add(a, v, false); break;          // ADD
            case 1: r = alu_add(a, v, true); break;           // ADC
            case 2: r = alu_sub(a, v, false); break;          // SUB
            case 3: r = alu_sub(a, v, cy); break;             // SBB
            case 4: r = alu_ana(a, v); break;                 // ANA
            case 5: r = alu_xra(a, v); break;                 // XRA
            case 6: r = alu_ora(a, v); break;                 // ORA
            default: r = alu_sub(a, v, false); break;         // CMP
        }
        cy = r.cy;
        ac = r.ac;
        set_szp(r.value);
        if (group != 7) a = r.value;   // CMP computes flags only
        cost = immediate ? 7 : ((opcode & 7) == REG_M ? 7 : 4);
        cycles += cost;
        return cost;
    }

    // NOP (0x00): pure time sink, 4 T-states.
    if (opcode == 0x00) {
        cost = 4;
        cycles += cost;
        return cost;
    }

    // HLT (0x76): halts the core; run loops check `halted`.
    if (opcode == 0x76) {
        halted = true;
        cost = 7;
        cycles += cost;
        return cost;
    }


    // JMP addr (C3, 10T) and conditional jumps Jcc (11CCC010): 10T taken,
    // 7T not taken — the fetch of the operand is what costs the extra
    // three states when the branch IS taken.
    if (opcode == 0xC3 || (opcode & 0xC7) == 0xC2) {
        const uint16_t target = fetch16();
        const bool taken =
            opcode == 0xC3 ? true : condition((opcode >> 3) & 7);
        if (taken) pc = target;
        cost = taken ? 10 : 7;
        cycles += cost;
        return cost;
    }

    // CALL addr (CD) and conditional calls Ccc (11CCC100):
    // 17T taken (push return address), 11T not taken.
    if (opcode == 0xCD || (opcode & 0xC7) == 0xC4) {
        const uint16_t target = fetch16();
        const bool taken =
            opcode == 0xCD ? true : condition((opcode >> 3) & 7);
        if (taken) {
            stack_push(*this, pc);
            pc = target;
//@LABS-BEGIN 1
//@LABS-SOLUTION
            cost = 17;   // full call: fetch address + push return PC
//@LABS-STUB
            // BUG(1): taken/not-taken cycle counts are SWAPPED. Symptom:
            // every executed call burns 11T instead of 17T, so cumulative
            // cyc runs low from the first conditional call onward and the
            // trace diverges exactly at that instruction.
            cost = 11;
//@LABS-END
        } else {
//@LABS-BEGIN 2
//@LABS-SOLUTION
            cost = 11;   // a skipped call still fetched its address operand
//@LABS-STUB
            // BUG(1) continued: the not-taken side charges the full 17T.
            cost = 17;
//@LABS-END
        }
        cycles += cost;
        return cost;
    }

    // RET (C9, 10T) and conditional returns Rcc (11CCC000):
    // 11T taken, only 5T when the condition fails.
    if (opcode == 0xC9 || (opcode & 0xC7) == 0xC0) {
        const bool conditional = opcode != 0xC9;
        const bool taken =
            conditional ? condition((opcode >> 3) & 7) : true;
        if (taken) {
            pc = stack_pop(*this);
            cost = conditional ? 11 : 10;
        } else {
            cost = 5;
        }
        cycles += cost;
        return cost;
    }

    // PUSH rp/PSW (11RP0101, 11T) and POP rp/PSW (11RP0001, 10T).
    // PSW pushes A in the high byte and packed flags in the low byte.
    if ((opcode & 0xCF) == 0xC5 || (opcode & 0xCF) == 0xC1) {
        // Low nibble separates PUSH (11RP0101) from POP (11RP0001).
        const bool is_push = (opcode & 0x0F) == 0x05;
        switch ((opcode >> 4) & 3) {
            case 0:  // BC
                if (is_push) stack_push(*this, bc()); else set_bc(stack_pop(*this));
                break;
            case 1:  // DE
                if (is_push) stack_push(*this, de()); else set_de(stack_pop(*this));
                break;
            case 2:  // HL
                if (is_push) stack_push(*this, hl()); else set_hl(stack_pop(*this));
                break;
            default: {  // PSW: A high, flags low
                if (is_push) {
                    stack_push(*this,
                               uint16_t(uint16_t(a) << 8 |
                                        pack_psw(s, z, ac, p, cy)));
                } else {
                    const uint16_t v = stack_pop(*this);
                    a = uint8_t(v >> 8);
                    const FlagsView f = unpack_psw(uint8_t(v));
                    s = f.s; z = f.z; ac = f.ac; p = f.p; cy = f.cy;
                }
                break;
            }
        }
        cost = is_push ? 11 : 10;
        cycles += cost;
        return cost;
    }

    // RST n (11NNN111): hardware call to n*8. The vector comes straight
    // from the opcode bits — no operand fetch. 11 T-states.
    if ((opcode & 0xC7) == 0xC7) {
        stack_push(*this, pc);
        pc = uint16_t((opcode >> 3) & 7) << 3;
        cost = 11;
        cycles += cost;
        return cost;
    }

    // PCHL (E9, 5T): jump through HL. SPHL (F9, 5T): move HL into SP.
    // XCHG (EB, 4T): swap DE with HL.
    if (opcode == 0xE9) {
        pc = hl();
        cost = 5;
        cycles += cost;
        return cost;
    }
    if (opcode == 0xF9) {
        sp = hl();
        cost = 5;
        cycles += cost;
        return cost;
    }
    if (opcode == 0xEB) {
        const uint16_t d = de();
        set_de(hl());
        set_hl(d);
        cost = 4;
        cycles += cost;
        return cost;
    }

    // EI (FB) / DI (F3), 4T each. Simplification vs silicon: the flag
    // change takes effect immediately instead of after the next
    // instruction (documented in the chapter lecture).
    if (opcode == 0xFB) {
        iff = true;
        cost = 4;
        cycles += cost;
        return cost;
    }
    if (opcode == 0xF3) {
        iff = false;
        cost = 4;
        cycles += cost;
        return cost;
    }
    // Unknown opcodes behave as NOPs in this subset but still burn 4 T-
    // states so trace cycle counts stay monotonic until later chapters
    // fill in the remaining groups.
    cost = 4;
    cycles += cost;
    return cost;
}

bool Cpu::interrupt(uint8_t opcode) {
    // Interrupt acknowledge: sampled only while IFF is set. Acceptance
    // pushes PC (so IRET-style RET resumes the interrupted stream), jumps
    // through the jammed opcode's RST vector, clears IFF and wakes HLT.
    if (!iff) return false;
    stack_push(*this, pc);
    pc = uint16_t(opcode & 0x38);
    iff = false;
    halted = false;
    return true;
}

}  // namespace i8080
