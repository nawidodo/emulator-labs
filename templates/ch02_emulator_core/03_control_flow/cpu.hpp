#pragma once
// LAB-8 CPU core — Chapter 2, exercise 03: control flow and memory access.
//
// Exercises 01/02 gave you the loop and the ALU; those parts are provided
// complete below. This exercise completes the LAB-8 instruction set: STORE,
// LOADM, JMP and JZ. The subtle parts are all in the PC contract: fetch()
// already left pc pointing at the NEXT instruction, so a jump simply
// overwrites pc — it must not advance it again.
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

// --- Given: fetch / decode / step / run ------------------------------------

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

// --- Given: ALU handlers from exercise 02 -----------------------------------

//@LABS-BEGIN 1
//@LABS-SOLUTION
// STORE r[a], ram[address]: register OUT to memory. Costs 6 cycles
// (memory writes are slower than register reads on real buses too).
inline void execute_store(Cpu& c, const Decoded& d, StepResult& res) {
    c.ram[d.b] = c.r[d.a];
    res.cycles = 6;
}
//@LABS-STUB
inline void execute_store(Cpu& c, const Decoded& d, StepResult& res) {
    // TODO(1): write c.r[d.a] to ram at the ADDRESS operand d.b.
    // Encoding reminder (SPEC.md): for STORE the first byte names the
    // register, the second the target address. Costs 6 cycles.
    res.cycles = 6;  // stub makes progress so a red test cannot hang
    (void)c;
    (void)d;
    (void)res;
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// LOADM r[a], ram[address]: memory IN to a register. Mirror of STORE.
inline void execute_loadm(Cpu& c, const Decoded& d, StepResult& res) {
    c.r[d.a] = c.ram[d.b];
    res.cycles = 6;
}
//@LABS-STUB
inline void execute_loadm(Cpu& c, const Decoded& d, StepResult& res) {
    // TODO(2): read ram at the ADDRESS operand d.b into register c.r[d.a].
    // Costs 6 cycles. Must not disturb any flags.
    res.cycles = 6;  // stub makes progress so a red test cannot hang
    (void)c;
    (void)d;
    (void)res;
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// JMP addr: unconditional jump. fetch() already advanced pc past this
// instruction, so the target REPLACES pc outright — adding the instruction
// length again is the classic double-advance bug (see 90_debug).
inline void execute_jmp(Cpu& c, const Decoded& d, StepResult& res) {
    c.pc = d.a;
    res.pc = c.pc;
    res.cycles = 3;
}
//@LABS-STUB
inline void execute_jmp(Cpu& c, const Decoded& d, StepResult& res) {
    // TODO(3): jump to the absolute address in d.a. Remember: fetch() has
    // already moved pc past the JMP, so pc <- d.a exactly, nothing more.
    // Keep res.pc in sync. Costs 3 cycles.
    res.cycles = 3;  // stub makes progress so a red test cannot hang
    (void)c;
    (void)d;
    (void)res;
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// JZ addr: conditional jump on the ZERO flag. Cycle cost differs by outcome
// (taken branches are more expensive) — that asymmetry is visible directly
// in trace files, which is why cyc is per-instruction.
inline void execute_jz(Cpu& c, const Decoded& d, StepResult& res) {
    if (c.zero) {
        c.pc = d.a;
        res.pc = c.pc;
        res.cycles = 3;
    } else {
        res.cycles = 2;  // fall through: fetch() already advanced pc
    }
}
//@LABS-STUB
inline void execute_jz(Cpu& c, const Decoded& d, StepResult& res) {
    // TODO(4): if c.zero is set, jump exactly like JMP (cost 3); otherwise
    // fall through (fetch() already advanced pc) at cost 2.
    res.cycles = 2;  // stub makes progress so a red test cannot hang
    (void)c;
    (void)d;
    (void)res;
}
//@LABS-END

// Full dispatch — the LAB-8 ISA is now complete.
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
