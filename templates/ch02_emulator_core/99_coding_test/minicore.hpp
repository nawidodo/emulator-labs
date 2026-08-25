#pragma once
// MiniCore-12 — Chapter 2 CODING TEST target.
//
// Implement the ISA specified in CODING_TEST.md. The test suite (main.cpp)
// is the executable form of that spec: make it pass without editing it.
#include <cstddef>
#include <cstdint>
#include <span>

namespace minicore {

enum class StepError : uint8_t {
    None = 0,
    UnknownOpcode,
    BadRegister,
};

struct StepResult {
    uint32_t cycles = 0;
    uint16_t pc = 0;     // pc AFTER the step
    StepError error = StepError::None;
};

// Decoded form of one 2-byte instruction word.
struct Decoded {
    uint8_t opcode = 0;  // high nibble of byte0
    uint8_t x = 0;       // low nibble of byte0 (destination register)
    uint8_t y = 0;       // high nibble of byte1 (source register, or 0)
    uint8_t nn = 0;      // byte1 as immediate/address
    StepError error = StepError::None;
};

struct Cpu {
    uint8_t r[4] = {0, 0, 0, 0};
    uint8_t mem[256] = {};
    uint8_t pc = 0;
    bool zf = false;
    bool cf = false;
    bool halted = false;

    void reset() { *this = Cpu{}; }

    void load(std::span<const uint8_t> program) {
        reset();
        for (std::size_t i = 0; i < program.size() && i < 256; ++i)
            mem[i] = program[i];
    }

    StepResult step();
    uint32_t run(uint32_t max_cycles);
};

// Given: run the universal loop over the step primitive.
inline uint32_t Cpu::run(uint32_t max_cycles) {
    uint32_t spent = 0;
    while (!halted && spent < max_cycles)
        spent += step().cycles;
    return spent;
}

namespace detail {

constexpr bool reg_ok(uint8_t nibble) {
    return nibble <= 3;
}

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Split the instruction word into fields and validate register nibbles.
// Register-carrying ops: LDI/MOV/ADD/SUB/INC/DEC/SHL/LD/ST validate x
// (and y for MOV/ADD/SUB). JMP/JNZ ignore x entirely. HLT ignores both.
inline Decoded decode(uint8_t b0, uint8_t b1) {
    Decoded d;
    d.opcode = b0 >> 4;
    d.x = b0 & 0x0F;
    d.y = b1 >> 4;
    d.nn = b1;
    switch (d.opcode) {
        case 0x8:  // JMP/JNZ ignore x entirely; HLT ignores both nibbles
        case 0x9:
        case 0xD:
            break;
        case 0x1:  // defined register-carrying ops validate their fields
        case 0x2:
        case 0x3:
        case 0x4:
        case 0x5:
        case 0x6:
        case 0x7:
        case 0xA:
        case 0xB:
            if (!reg_ok(d.x) ||
                ((d.opcode == 0x2 || d.opcode == 0x3 || d.opcode == 0x4) &&
                 !reg_ok(d.y))) {
                d.error = StepError::BadRegister;
            }
            break;
        default:   // 0x0, 0xC, 0xE, 0xF are undefined (CODING_TEST.md)
            d.error = StepError::UnknownOpcode;
            break;
    }
    return d;
}
//@LABS-STUB
inline Decoded decode(uint8_t b0, uint8_t b1) {
    (void)b1;
    // TODO(1): split the word per CODING_TEST.md — opcode is the HIGH nibble
    // of b0, x its LOW nibble, y the HIGH nibble of b1, nn all of b1.
    // Undefined opcodes (not 0x1..0xB, 0xD) -> UnknownOpcode.
    // Reserved register nibbles (> 3): x for all ops except JMP/JNZ/HLT,
    // additionally y for MOV/ADD/SUB -> BadRegister.
    return Decoded{};  // wrong on purpose
}
//@LABS-END

}  // namespace detail

//@LABS-BEGIN 2
//@LABS-SOLUTION
// LDI / MOV / ADD / SUB. Flags come from the wide result, exactly as the
// spec table says.
inline void execute_alu(Cpu& c, const Decoded& d, StepResult& res) {
    switch (d.opcode) {
        case 0x1:  // LDI
            c.r[d.x] = d.nn;
            res.cycles = 2;
            break;
        case 0x2:  // MOV
            c.r[d.x] = c.r[d.y];
            res.cycles = 2;
            break;
        case 0x3: {  // ADD
            const uint16_t sum = uint16_t(c.r[d.x]) + uint16_t(c.r[d.y]);
            c.r[d.x] = uint8_t(sum);
            c.zf = c.r[d.x] == 0;
            c.cf = sum > 0xFF;
            res.cycles = 2;
            break;
        }
        default: {  // 0x4 SUB
            const bool borrow = c.r[d.x] < c.r[d.y];
            c.r[d.x] = uint8_t(c.r[d.x] - c.r[d.y]);
            c.zf = c.r[d.x] == 0;
            c.cf = borrow;
            res.cycles = 2;
            break;
        }
    }
}
//@LABS-STUB
inline void execute_alu(Cpu& c, const Decoded& d, StepResult& res) {
    // TODO(2): implement LDI(0x1), MOV(0x2), ADD(0x3), SUB(0x4) with the
    // exact flag semantics from the CODING_TEST.md table. Each costs 2
    // cycles. Charge them here so a red test cannot hang run().
    res.cycles = 2;
    (void)c;
    (void)d;
    (void)res;
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// INC / DEC / SHL. Watch the asymmetries: INC never touches cf, DEC sets cf
// only on the wrap from 0x00 to 0xFF, SHL moves bit 7 into cf.
inline void execute_unary(Cpu& c, const Decoded& d, StepResult& res) {
    switch (d.opcode) {
        case 0x5: {  // INC
            c.r[d.x] = uint8_t(c.r[d.x] + 1);
            c.zf = c.r[d.x] == 0;
            res.cycles = 2;
            break;
        }
        case 0x6: {  // DEC
            const bool wrapped = c.r[d.x] == 0x00;
            c.r[d.x] = uint8_t(c.r[d.x] - 1);
            c.zf = c.r[d.x] == 0;
            c.cf = wrapped;
            res.cycles = 2;
            break;
        }
        default: {  // 0x7 SHL
            c.cf = (c.r[d.x] & 0x80) != 0;
            c.r[d.x] = uint8_t(c.r[d.x] << 1);
            c.zf = c.r[d.x] == 0;
            res.cycles = 2;
            break;
        }
    }
}
//@LABS-STUB
inline void execute_unary(Cpu& c, const Decoded& d, StepResult& res) {
    // TODO(3): implement INC(0x5), DEC(0x6), SHL(0x7) with the exact flag
    // semantics from the CODING_TEST.md table. Each costs 2 cycles.
    res.cycles = 2;  // stub makes progress so a red test cannot hang run()
    (void)c;
    (void)d;
    (void)res;
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// JMP / JNZ / LD / ST / HLT — control flow and memory.
inline void execute_flow(Cpu& c, const Decoded& d, StepResult& res) {
    switch (d.opcode) {
        case 0x8:  // JMP
            c.pc = d.nn;
            res.pc = c.pc;
            res.cycles = 2;
            break;
        case 0x9:  // JNZ
            if (!c.zf) {
                c.pc = d.nn;
                res.pc = c.pc;
                res.cycles = 2;
            } else {
                res.cycles = 1;
            }
            break;
        case 0xA:  // LD
            c.r[d.x] = c.mem[d.nn];
            res.cycles = 3;
            break;
        case 0xB:  // ST
            c.mem[d.nn] = c.r[d.x];
            res.cycles = 3;
            break;
        default:   // 0xD HLT
            c.halted = true;
            res.cycles = 1;
            break;
    }
}
//@LABS-STUB
inline void execute_flow(Cpu& c, const Decoded& d, StepResult& res) {
    // TODO(4): implement JMP(0x8), JNZ(0x9), LD(0xA), ST(0xB), HLT(0xD).
    // Cycle costs: JMP 2, JNZ 2 taken / 1 not taken, LD/ST 3, HLT 1.
    res.cycles = 3;  // stub makes progress so a red test cannot hang run()
    (void)c;
    (void)d;
    (void)res;
}
//@LABS-END

// Given: the universal loop wiring for MiniCore-12.
inline StepResult Cpu::step() {
    StepResult res;
    res.pc = pc;
    if (halted)
        return res;

    // Every MiniCore-12 instruction is exactly two bytes; pc wraps mod 256.
    const uint8_t b0 = mem[pc];
    const uint8_t b1 = mem[static_cast<uint8_t>(pc + 1)];
    pc = static_cast<uint8_t>(pc + 2);
    res.pc = pc;

    const Decoded d = detail::decode(b0, b1);
    if (d.error != StepError::None) {
        res.cycles = (d.error == StepError::UnknownOpcode) ? 1u : 2u;
        res.error = d.error;
        halted = true;
        return res;
    }

    switch (d.opcode) {
        case 0x1:
        case 0x2:
        case 0x3:
        case 0x4:
            execute_alu(*this, d, res);
            break;
        case 0x5:
        case 0x6:
        case 0x7:
            execute_unary(*this, d, res);
            break;
        default:
            execute_flow(*this, d, res);
            break;
    }
    return res;
}

}  // namespace minicore
