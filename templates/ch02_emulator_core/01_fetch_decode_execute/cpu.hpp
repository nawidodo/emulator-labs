#pragma once
// LAB-8 CPU core — Chapter 2, exercise 01: the fetch/decode/execute skeleton.
//
// The machine state below matches SPEC.md. This exercise builds the universal
// loop itself: fetch() gathers raw bytes, decode() interprets them, execute()
// mutates state, step() wires the three together. Later exercises fill in the
// rest of the instruction set inside execute().
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
    UnknownOpcode,  // opcode not in the ISA table
    BadRegister,    // reserved register field (> 3)
};

// Result of ONE executed instruction. Every layer of every emulator in this
// course hangs off this: unit tests, tracing, scheduling.
struct StepResult {
    uint32_t cycles = 0;   // cycles consumed by THIS instruction
    uint16_t pc = 0;       // pc AFTER the step (address of next instruction)
    StepError error = StepError::None;
};

// Raw bytes gathered by fetch(), before any interpretation.
struct RawInsn {
    uint8_t opcode = 0;
    uint8_t b0 = 0;  // first operand byte (register index or address)
    uint8_t b1 = 0;  // second operand byte (register index or immediate)
    uint8_t len = 1;
};

// Decode output: what to do + validated operands.
struct Decoded {
    Op op = Op::Halt;
    uint8_t a = 0;
    uint8_t b = 0;
    StepError error = StepError::None;
};

// Encoded instruction length in bytes; 0 marks an undefined opcode.
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
    // Architectural state — visible to programs, part of every trace/dump.
    uint8_t r[4] = {0, 0, 0, 0};
    uint8_t ram[256] = {};
    uint8_t pc = 0;
    bool zero = false;
    bool carry = false;
    bool halted = false;

    void reset() { *this = Cpu{}; }

    // Copy a program image to ram[0x00..] and start executing at 0x00.
    void load(std::span<const uint8_t> program) {
        reset();
        for (std::size_t i = 0; i < program.size() && i < 256; ++i)
            ram[i] = program[i];
    }

    RawInsn fetch();
    StepResult step();
    uint32_t run(uint32_t max_cycles);
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Read the opcode at pc, gather its operand bytes (addresses wrap mod 256),
// and leave pc pointing at the NEXT instruction. Undefined encodings have no
// operand bytes: they are fetched as a bare opcode so decode can reject them.
inline RawInsn Cpu::fetch() {
    RawInsn raw;
    raw.opcode = ram[pc];
    raw.len = insn_len(raw.opcode);
    if (raw.len == 0)
        raw.len = 1;
    for (unsigned i = 1; i < raw.len; ++i) {
        // PC wraparound is defined behavior, not an error (SPEC.md).
        const uint8_t addr = static_cast<uint8_t>(pc + i);
        if (i == 1)
            raw.b0 = ram[addr];
        else
            raw.b1 = ram[addr];
    }
    pc = static_cast<uint8_t>(pc + raw.len);
    return raw;
}
//@LABS-STUB
inline RawInsn Cpu::fetch() {
    (void)this;
    // TODO(1): read the opcode byte at pc, look up its length with
    // insn_len() (an undefined opcode counts as length 1), gather the
    // operand bytes b0/b1 honoring PC wraparound, then advance pc past the
    // whole instruction so it points at the next one.
    return RawInsn{};  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Interpret raw bytes: opcode -> Op, plus validation of register fields.
// Reserved encodings become explicit errors here, never silent modulo tricks.
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
            break;  // Halt/Jmp/Jz take no register fields
    }
    return d;
}
//@LABS-STUB
inline Decoded decode(const RawInsn& raw) {
    (void)raw;
    // TODO(2): map raw.opcode to the matching Op value (see enum Op and
    // SPEC.md). Unknown opcodes must yield error = StepError::UnknownOpcode.
    // For Load/Store/Loadm/ADD/SUB, copy the operand bytes into a/b and flag
    // StepError::BadRegister when any register field exceeds 3.
    return Decoded{};  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// Mutate machine state for one decoded instruction. This exercise implements
// HALT and LOAD only; later exercises extend the switch. Unimplemented-but-
// defined opcodes deliberately fall into the defensive default so a half-
// finished core halts loudly instead of executing nonsense.
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
        default:
            c.halted = true;
            res.error = StepError::UnknownOpcode;
            res.cycles = 2;
            break;
    }
}
//@LABS-STUB
inline void execute(Cpu& c, const Decoded& d, StepResult& res) {
    // TODO(3): dispatch on d.op:
    //   Op::Halt -> c.halted = true, res.cycles = 4
    //   Op::Load -> c.r[d.a] = d.b, res.cycles = 4
    //   default  -> c.halted = true, res.error = StepError::UnknownOpcode,
    //               res.cycles = 2
    (void)c;
    (void)d;
    (void)res;
}
//@LABS-END

// The universal loop, one instruction at a time. On a decode error the
// instruction's effects do NOT happen; the machine halts deterministically
// (SPEC.md: fetch costs 2 cycles, bad-register aborts cost 4).
//@LABS-BEGIN 4
//@LABS-SOLUTION
inline StepResult Cpu::step() {
    StepResult res;
    res.pc = pc;
    if (halted)
        return res;  // stepping a halted machine is a documented no-op

    const RawInsn raw = fetch();
    res.pc = pc;  // fetch left pc at the next instruction
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
//@LABS-STUB
inline StepResult Cpu::step() {
    // TODO(4): wire the loop together:
    //   - a halted machine returns immediately (zero cycles, current pc)
    //   - fetch(); record the new pc into res.pc
    //   - decode(); on error charge 2 cycles for UnknownOpcode / 4 for
    //     BadRegister, set halted, and return without executing
    //   - otherwise execute(*this, d, res)
    return StepResult{};  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 5
//@LABS-SOLUTION
// Run until HALT, an error, or the cycle budget runs out. Returns the cycles
// actually spent. Budget exhaustion leaves the machine running (callers
// decide whether that is a hang).
inline uint32_t Cpu::run(uint32_t max_cycles) {
    uint32_t spent = 0;
    while (!halted && spent < max_cycles)
        spent += step().cycles;
    return spent;
}
//@LABS-STUB
inline uint32_t Cpu::run(uint32_t max_cycles) {
    (void)max_cycles;
    // TODO(5): call step() repeatedly until halted or max_cycles is reached;
    // return the total cycles spent.
    return 0;  // wrong on purpose
}
//@LABS-END

}  // namespace ch02
