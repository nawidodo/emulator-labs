#pragma once
// LAB-8 CPU core — Chapter 2, exercise 90: DEBUGGING exercise.
//
// This core is COMPLETE but two handlers misbehave. The test suite below
// pins the correct behavior from SPEC.md and goes red until you find and fix
// both seeded bugs. Do NOT rewrite the core: each fix is one or two lines.
// Document your findings in bug-report.md (see DEBUGGING.md).
#include <cstddef>
#include <cstdint>
#include <span>

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

inline void execute_store(Cpu& c, const Decoded& d, StepResult& res) {
    c.ram[d.b] = c.r[d.a];
    res.cycles = 6;
}

inline void execute_loadm(Cpu& c, const Decoded& d, StepResult& res) {
    c.r[d.a] = c.ram[d.b];
    res.cycles = 6;
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

inline void execute(Cpu& c, const Decoded& d, StepResult& res);

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

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline void execute_jmp(Cpu& c, const Decoded& d, StepResult& res) {
    c.pc = d.a;
    res.pc = c.pc;
    res.cycles = 3;
}
//@LABS-STUB
inline void execute_jmp(Cpu& c, const Decoded& d, StepResult& res) {
    // Seeded BUG #1 lives somewhere in here. TODO(1): compare against
    // SPEC.md ("the target replaces the pc outright") and think about what
    // fetch() already did to pc before execute ran.
    c.pc = d.a;
    c.pc = static_cast<uint8_t>(c.pc + 1);  // <- suspicious?
    res.pc = c.pc;
    res.cycles = 3;
}
//@LABS-END


//@LABS-BEGIN 2
//@LABS-SOLUTION
inline void execute_sub(Cpu& c, const Decoded& d, StepResult& res) {
    const bool borrow = c.r[d.a] < c.r[d.b];
    c.r[d.a] = uint8_t(c.r[d.a] - c.r[d.b]);
    c.zero = c.r[d.a] == 0;
    c.carry = borrow;
    res.cycles = 4;
}
//@LABS-STUB
inline void execute_sub(Cpu& c, const Decoded& d, StepResult& res) {
    // Seeded BUG #2 lives somewhere in here. TODO(2): SPEC.md says C is set
    // on BORROW. Check what this code actually computes, including for the
    // equal-operands edge case.
    const bool borrow = c.r[d.a] >= c.r[d.b];  // <- suspicious?
    c.r[d.a] = uint8_t(c.r[d.a] - c.r[d.b]);
    c.zero = c.r[d.a] == 0;
    c.carry = borrow;
    res.cycles = 4;
}
//@LABS-END

inline void execute(Cpu& c, const Decoded& d, StepResult& res) {
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

}  // namespace ch02
