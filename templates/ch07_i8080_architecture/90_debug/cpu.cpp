#include "cpu.hpp"
#include <cstdio>

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

    // LXI rp,d16 : 00RP0001 — rp field selects BC(00)/DE(01)/HL(10);
    // SP(11) belongs to chapter 8's stack work. 10 T-states.
    if ((opcode & 0xCF) == 0x01 && (opcode & 0x30) != 0x30) {
        const uint16_t imm = fetch16();
        switch ((opcode >> 4) & 3) {
            case 0: set_bc(imm); break;
            case 1: set_de(imm); break;
            case 2: set_hl(imm); break;
        }
        cost = 10;
        cycles += cost;
        return cost;
    }

    // LDA addr / STA addr : direct A <-> memory, 16-bit address operand.
    if (opcode == 0x3A) {
//@LABS-BEGIN 3
//@LABS-SOLUTION
        a = bus ? bus->read(fetch16()) : 0;
//@LABS-STUB
        // BUG(3): LDA reads its address high-byte-first. Assemblers emit
        // low, then high (8080 is little-endian), so this loads from a
        // byte-swapped location: LDA 0x2000 actually touches 0x0020 and
        // programs see stale zeros instead of their data tables.
        {
            const uint8_t hi = fetch8();
            const uint8_t lo = fetch8();
            a = bus ? bus->read(uint16_t(uint16_t(hi) << 8 | lo))
                    : 0;  // wrong on purpose
        }
//@LABS-END
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
//@LABS-BEGIN 1
//@LABS-SOLUTION
        set_reg(dst, new_v);
        ac = inc ? aux_carry_add(old_v, 0x01, false)
                 : aux_carry_sub(old_v, 0x01, false);
        set_szp(new_v);
        cost = (dst == REG_M) ? 10 : 5;
//@LABS-STUB
        // BUG(1): DCR treats a borrow-out as carry. The real 8080
        // PRESERVES CY across INR/DCR (only STC/CMC/ALU groups touch it),
        // so any countdown loop using DEC+CPI then branching on CY after
        // this fix diverges the moment DCR underflows.
        set_reg(dst, new_v);
        ac = inc ? aux_carry_add(old_v, 0x01, false)
                 : aux_carry_sub(old_v, 0x01, false);
        if (!inc && old_v == 0x00) cy = true;  // wrong on purpose
        set_szp(new_v);
        cost = (dst == REG_M) ? 10 : 5;
//@LABS-END
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
            case 0: r = alu_add(a, v, cy); break;             // ADD
            case 1: r = alu_add(a, v, true); break;           // ADC
            case 2: r = alu_sub(a, v, false); break;          // SUB
            case 3: r = alu_sub(a, v, cy); break;             // SBB
            case 4: r = alu_ana(a, v); break;                 // ANA
            case 5: r = alu_xra(a, v); break;                 // XRA
            case 6: r = alu_ora(a, v); break;                 // ORA
            default: r = alu_sub(a, v, false); break;         // CMP
        }
//@LABS-BEGIN 2
//@LABS-SOLUTION
        cy = r.cy;
        ac = r.ac;
        set_szp(r.value);
        if (group != 7) a = r.value;   // CMP computes flags only
//@LABS-STUB
        // BUG(2): CMP writes its result into A. Every CPI comparison in a
        // program destroys the accumulator, so loops comparing against a
        // constant see A == constant after the first iteration and the
        // branch pattern changes completely.
        cy = r.cy;
        ac = r.ac;
        set_szp(r.value);
        a = r.value;   // wrong on purpose (also clobbers A on CMP)
//@LABS-END
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

    // Unknown opcodes behave as NOPs in this subset but still burn 4 T-
    // states so trace cycle counts stay monotonic until later chapters
    // fill in the remaining groups.
    cost = 4;
    cycles += cost;
    return cost;
}

std::string Cpu::disassemble(uint16_t at) const {
    auto rd = [&](uint16_t addr) -> uint8_t {
        return bus ? bus->read(addr) : 0;
    };
    static const char* kAluName[8] = {"ADD", "ADC", "SUB", "SBB",
                                      "ANA", "XRA", "ORA", "CMP"};
    static const char* kAluImmName[8] = {"ADI", "ACI", "SUI", "SBI",
                                         "ANI", "XRI", "ORI", "CPI"};
    static const char* kReg[8] = {"B", "C", "D", "E", "H", "L", "M", "A"};

    const uint8_t op = rd(at);
    char buf[32];

    if (op == 0x00) return "NOP";
    if (op == 0x76) return "HLT";
    if (op == 0x3A) {
        snprintf(buf, sizeof buf, "LDA %04X",
                 unsigned(rd(uint16_t(at + 1)) |
                          rd(uint16_t(at + 2)) << 8));
        return buf;
    }
    if (op == 0x32) {
        snprintf(buf, sizeof buf, "STA %04X",
                 unsigned(rd(uint16_t(at + 1)) |
                          rd(uint16_t(at + 2)) << 8));
        return buf;
    }
    if ((op & 0xC0) == 0x40 && op != 0x76) {
        snprintf(buf, sizeof buf, "MOV %s,%s",
                 kReg[(op >> 3) & 7], kReg[op & 7]);
        return buf;
    }
    if ((op & 0xC7) == 0x06) {
        snprintf(buf, sizeof buf, "MVI %s,#%02X",
                 kReg[(op >> 3) & 7], unsigned(rd(uint16_t(at + 1))));
        return buf;
    }
    if ((op & 0xC7) == 0x04 || (op & 0xC7) == 0x05) {
        snprintf(buf, sizeof buf, "%s %s",
                 (op & 0x07) == 0x04 ? "INR" : "DCR", kReg[(op >> 3) & 7]);
        return buf;
    }
    if ((op & 0xC0) == 0x80) {
        snprintf(buf, sizeof buf, "%s %s", kAluName[(op >> 3) & 7],
                 kReg[op & 7]);
        return buf;
    }
    if ((op & 0xC7) == 0xC6) {
        snprintf(buf, sizeof buf, "%s #%02X", kAluImmName[(op >> 3) & 7],
                 unsigned(rd(uint16_t(at + 1))));
        return buf;
    }
    snprintf(buf, sizeof buf, "DB %02X", unsigned(op));
    return buf;
}

}  // namespace i8080
