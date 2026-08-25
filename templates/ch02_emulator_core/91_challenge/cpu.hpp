#pragma once
// LAB-8X CPU core — Chapter 2 CHALLENGE: subroutines via CALL/RET and a
// hardware stack in RAM.
//
// The base LAB-8 core is given complete (exercises 01–03 solutions). Your
// job is the extension documented in CHALLENGE.md: a downward-growing stack
// at the top of RAM plus two new instructions, CALL (0x80) and RET (0x90).
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
    Call  = 0x80,  // challenge extension
    Ret   = 0x90,  // challenge extension
};

enum class StepError : uint8_t {
    None = 0,
    UnknownOpcode,
    BadRegister,
    StackOverflow,   // push onto a full stack (sp == 0x00)
    StackUnderflow,  // pop from an empty stack (sp == 0xFF)
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
        case 0x70:
        case 0x80: return 2;
        case 0x90: return 1;
        default: return 0;
    }
}

struct Cpu {
    uint8_t r[4] = {0, 0, 0, 0};
    uint8_t ram[256] = {};
    uint8_t pc = 0;
    uint8_t sp = 0xFF;  // index of the next FREE stack byte; 0xFF == empty
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

    StepError push(uint8_t value);
    StepError pop(uint8_t* out);
};

// --- Given: base core --------------------------------------------------------

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
        case 0x80: d.op = Op::Call; break;
        case 0x90: d.op = Op::Ret; break;
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

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Push one byte onto the stack. The stack grows DOWN from the top of RAM:
// sp names the next free slot (starts at 0xFF == empty), so a push stores
// at sp and then decrements. Pushing when sp == 0x00 would wrap into the
// program area — that is a hard StackOverflow instead.
inline StepError Cpu::push(uint8_t value) {
    if (sp == 0x00)
        return StepError::StackOverflow;
    ram[sp] = value;
    --sp;
    return StepError::None;
}
//@LABS-STUB
inline StepError Cpu::push(uint8_t value) {
    // TODO(1): implement the downward-growing push (see comment above).
    // Return StepError::StackOverflow when there is no room left.
    (void)value;
    return StepError::StackOverflow;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Pop one byte: increment first, then read. Popping an empty stack
// (sp == 0xFF) is a hard StackUnderflow.
inline StepError Cpu::pop(uint8_t* out) {
    if (sp == 0xFF)
        return StepError::StackUnderflow;
    ++sp;
    *out = ram[sp];
    return StepError::None;
}
//@LABS-STUB
inline StepError Cpu::pop(uint8_t* out) {
    // TODO(2): implement pop as the exact mirror of push, returning
    // StepError::StackUnderflow on an empty stack.
    (void)out;
    return StepError::StackUnderflow;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// CALL addr: push the return address (fetch() already left pc pointing at
// the instruction AFTER the CALL), then jump. A failed push halts the
// machine with the error — never continue with a corrupt frame.
inline void execute_call(Cpu& c, const Decoded& d, StepResult& res) {
    const StepError err = c.push(c.pc);
    if (err != StepError::None) {
        c.halted = true;
        res.error = err;
        res.cycles = 6;
        return;
    }
    c.pc = d.a;
    res.pc = c.pc;
    res.cycles = 6;
}
//@LABS-STUB
inline void execute_call(Cpu& c, const Decoded& d, StepResult& res) {
    // TODO(3): push c.pc (the return address), then jump exactly like JMP.
    // On push failure: set halted, put the error in res.error, do NOT jump.
    // Costs 6 cycles either way. Charge them here so a red test cannot hang.
    res.cycles = 6;
    (void)c;
    (void)d;
    (void)res;
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// RET: pop the return address straight into the pc. Popping an empty stack
// is a hard StackUnderflow halt — real CPUs fault here too.
inline void execute_ret(Cpu& c, StepResult& res) {
    uint8_t addr = 0;
    const StepError err = c.pop(&addr);
    if (err != StepError::None) {
        c.halted = true;
        res.error = err;
        res.cycles = 6;
        return;
    }
    c.pc = addr;
    res.pc = c.pc;
    res.cycles = 6;
}
//@LABS-STUB
inline void execute_ret(Cpu& c, StepResult& res) {
    // TODO(4): pop into the pc; on underflow halt with res.error set.
    // Costs 6 cycles either way. Charge them here so a red test cannot hang.
    res.cycles = 6;
    (void)c;
    (void)res;
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
        case Op::Call:
            execute_call(c, d, res);
            break;
        case Op::Ret:
            execute_ret(c, res);
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

    if (d.error != StepError::None &&
        (d.error == StepError::UnknownOpcode ||
         d.error == StepError::BadRegister)) {
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

}  // namespace ch02
