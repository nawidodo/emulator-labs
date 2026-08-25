#pragma once
// LAB-8 CPU core — Chapter 2, exercise 04: complete reference core plus the
// observability layer every emulator ships with: canonical trace formatting
// and a disassembler (house rule: disassemblers arrive early).
//
// This exercise adds no new ISA behavior; its job is running REAL programs
// (committed .bin images with .asm.txt provenance) through the headless
// runner and diffing traces against goldens.
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace ch02 {

enum class Op : uint8_t {
    Halt  = 0x00,
    Load  = 0x10,
    Store = 0x20,
    Loadm = 0x30,
    Add   = 0x40,
    Sub   = 0x50,
    Jmp   = 0x60,
    Jz    = 0x70,
};

enum class StepError : uint8_t {
    None = 0,
    UnknownOpcode,
    BadRegister,
};

struct StepResult {
    uint32_t cycles = 0;
    uint16_t pc = 0;
    StepError error = StepError::None;
};

struct RawInsn {
    uint8_t opcode = 0;
    uint8_t b0 = 0;
    uint8_t b1 = 0;
    uint8_t len = 1;
};

struct Decoded {
    Op op = Op::Halt;
    uint8_t a = 0;
    uint8_t b = 0;
    StepError error = StepError::None;
};

constexpr uint8_t insn_len(uint8_t opcode) {
    switch (opcode) {
        case 0x00: return 1;
        case 0x10:
        case 0x20:
        case 0x30:
        case 0x40:
        case 0x50: return 3;
        case 0x60:
        case 0x70: return 2;
        default: return 0;
    }
}

struct Cpu {
    uint8_t r[4] = {0, 0, 0, 0};
    uint8_t ram[256] = {};
    uint8_t pc = 0;
    bool zero = false;
    bool carry = false;
    bool halted = false;

    void reset() { *this = Cpu{}; }

    void load(std::span<const uint8_t> program) {
        reset();
        for (std::size_t i = 0; i < program.size() && i < 256; ++i)
            ram[i] = program[i];
    }

    RawInsn fetch();
    StepResult step();
    uint32_t run(uint32_t max_cycles);

    void execute(Cpu& c, const Decoded& d, StepResult& res);  // dispatch
};

inline RawInsn Cpu::fetch() {
    RawInsn raw;
    raw.opcode = ram[pc];
    raw.len = insn_len(raw.opcode);
    if (raw.len == 0)
        raw.len = 1;
    for (unsigned i = 1; i < raw.len; ++i) {
        const uint8_t addr = static_cast<uint8_t>(pc + i);
        if (i == 1)
            raw.b0 = ram[addr];
        else
            raw.b1 = ram[addr];
    }
    pc = static_cast<uint8_t>(pc + raw.len);
    return raw;
}

inline Decoded decode(const RawInsn& raw) {
    Decoded d;
    switch (raw.opcode) {
        case 0x00: d.op = Op::Halt; break;
        case 0x10: d.op = Op::Load; break;
        case 0x20: d.op = Op::Store; break;
        case 0x30: d.op = Op::Loadm; break;
        case 0x40: d.op = Op::Add; break;
        case 0x50: d.op = Op::Sub; break;
        case 0x60: d.op = Op::Jmp; break;
        case 0x70: d.op = Op::Jz; break;
        default:
            d.error = StepError::UnknownOpcode;
            return d;
    }
    const auto reg_ok = [](uint8_t v) { return v <= 3; };
    d.a = raw.b0;
    d.b = raw.b1;
    switch (d.op) {
        case Op::Load:
        case Op::Store:
        case Op::Loadm:
            if (!reg_ok(raw.b0))
                d.error = StepError::BadRegister;
            break;
        case Op::Add:
        case Op::Sub:
            if (!reg_ok(raw.b0) || !reg_ok(raw.b1))
                d.error = StepError::BadRegister;
            break;
        default:
            break;
    }
    return d;
}

inline void execute_add(Cpu& c, const Decoded& d, StepResult& res) {
    const uint16_t sum = uint16_t(c.r[d.a]) + uint16_t(c.r[d.b]);
    c.r[d.a] = uint8_t(sum);
    c.zero = c.r[d.a] == 0;   // Z reads the truncated ALU result
    c.carry = sum > 0xFF;     // C reads the carry above bit 7
    res.cycles = 4;
}

inline void execute_sub(Cpu& c, const Decoded& d, StepResult& res) {
    const bool borrow = c.r[d.a] < c.r[d.b];
    c.r[d.a] = uint8_t(c.r[d.a] - c.r[d.b]);
    c.zero = c.r[d.a] == 0;
    c.carry = borrow;
    res.cycles = 4;
}

inline void execute_store(Cpu& c, const Decoded& d, StepResult& res) {
    c.ram[d.b] = c.r[d.a];
    res.cycles = 6;
}

inline void execute_loadm(Cpu& c, const Decoded& d, StepResult& res) {
    c.r[d.a] = c.ram[d.b];
    res.cycles = 6;
}

inline void execute_jmp(Cpu& c, const Decoded& d, StepResult& res) {
    c.pc = d.a;
    res.pc = c.pc;
    res.cycles = 3;
}

inline void execute_jz(Cpu& c, const Decoded& d, StepResult& res) {
    if (c.zero) {
        c.pc = d.a;
        res.pc = c.pc;
        res.cycles = 3;
    } else {
        res.cycles = 2;
    }
}

inline void Cpu::execute(Cpu& c, const Decoded& d, StepResult& res) {
    switch (d.op) {
        case Op::Halt:
            c.halted = true;
            res.cycles = 4;
            break;
        case Op::Load:
            c.r[d.a] = d.b;
            res.cycles = 4;
            break;
        case Op::Store:
            execute_store(c, d, res);
            break;
        case Op::Loadm:
            execute_loadm(c, d, res);
            break;
        case Op::Add:
            execute_add(c, d, res);
            break;
        case Op::Sub:
            execute_sub(c, d, res);
            break;
        case Op::Jmp:
            execute_jmp(c, d, res);
            break;
        case Op::Jz:
            execute_jz(c, d, res);
            break;
    }
}

inline StepResult Cpu::step() {
    StepResult res;
    res.pc = pc;
    if (halted)
        return res;

    const RawInsn raw = fetch();
    res.pc = pc;
    const Decoded d = decode(raw);

    if (d.error != StepError::None) {
        res.cycles = (d.error == StepError::UnknownOpcode) ? 2u : 4u;
        res.error = d.error;
        halted = true;
        return res;
    }

    execute(*this, d, res);
    return res;
}

inline uint32_t Cpu::run(uint32_t max_cycles) {
    uint32_t spent = 0;
    while (!halted && spent < max_cycles)
        spent += step().cycles;
    return spent;
}

// --- Observability -----------------------------------------------------------

// Two-digit uppercase hex helper (trace fields are fixed-width).
inline std::string hex2(uint8_t v) {
    static constexpr char kDigits[] = "0123456789ABCDEF";
    std::string s;
    s += kDigits[v >> 4];
    s += kDigits[v & 0x0F];
    return s;
}

// One canonical trace line (SPEC.md), captured BEFORE executing:
//   pc=<hex> op=<hex> r0=<hex> r1=<hex> r2=<hex> r3=<hex> cyc=<decimal>
// `cyc` comes from the step result that consumed this instruction.
inline std::string format_trace(uint8_t pc, uint8_t opcode,
                                const uint8_t (&regs)[4], uint32_t cycles) {
    std::string line = "pc=" + hex2(pc) + " op=" + hex2(opcode);
    for (int i = 0; i < 4; ++i)
        line += " r" + std::to_string(i) + "=" + hex2(regs[i]);
    line += " cyc=" + std::to_string(cycles);
    return line;
}

// Disassembles ONE instruction starting at addr (house rule: every core gets
// a disassembler early — it is how you read traces and core dumps).
// Returns text like "LOAD r0, #0x07", "JMP 0x06" or ".byte 0x99" for data /
// undefined encodings.
inline std::string disassemble(const Cpu& c, uint8_t addr) {
    const uint8_t op = c.ram[addr];
    const uint8_t b0 = c.ram[static_cast<uint8_t>(addr + 1)];
    const uint8_t b1 = c.ram[static_cast<uint8_t>(addr + 2)];
    switch (op) {
        case 0x00: return "HALT";
        case 0x10: return "LOAD r" + std::to_string(b0) +
                         ", #" + hex2(b1);
        case 0x20: return "STORE r" + std::to_string(b0) +
                          ", 0x" + hex2(b1);
        case 0x30: return "LOADM r" + std::to_string(b0) +
                           ", 0x" + hex2(b1);
        case 0x40: return "ADD r" + std::to_string(b0) +
                          ", r" + std::to_string(b1);
        case 0x50: return "SUB r" + std::to_string(b0) +
                          ", r" + std::to_string(b1);
        case 0x60: return "JMP 0x" + hex2(b0);
        case 0x70: return "JZ 0x" + hex2(b0);
        default:   return ".byte 0x" + hex2(op);
    }
}

}  // namespace ch02
