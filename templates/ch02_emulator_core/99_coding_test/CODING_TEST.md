# CODING TEST — MiniCore-12

Implement an unseen fictional CPU from this specification alone. Everything
you need is on this page; the accompanying test suite (`main.cpp`) is the
executable form of this spec. Timebox: one focused session.

## Rules

- Work in the generated skeleton of this directory
  (`LABS=ch02_emulator_core/99_coding_test make skels`).
- Implement `minicore.hpp` until all tests pass. Do not modify `main.cpp`.
- The hidden grader compiles YOUR sources and runs the test binary; any
  failing test fails the gate.

## Machine model

```cpp
struct Cpu {
    uint8_t r[4];      // registers r0..r3
    uint8_t mem[256];  // flat RAM
    uint8_t pc;        // wraps mod 256
    bool zf;           // zero flag
    bool cf;           // carry flag
    bool halted;
};
```

- Every instruction is exactly **two bytes**: `byte0 = (opcode << 4) | reg_x`,
  `byte1 = operand` (register index in its HIGH nibble where used, otherwise
  an immediate or address).
- Programs load at address 0; pc starts at 0 and advances by 2 per
  instruction (all arithmetic on pc wraps mod 256).
- Flags are written ONLY by ADD, SUB, INC, DEC and SHL, exactly as specified
  below. Every other instruction preserves them.
- Register fields must be 0–3. A reserved register field (> 3) is an error,
  never a modulo.

## Instruction set (12)

| Op | Mnemonic | Encoding | Cycles | Semantics |
|---:|----------|----------|:------:|-----------|
| 0x1 | LDI  | `1x nn` | 2 | `r[x] ← nn`. Flags unchanged. |
| 0x2 | MOV  | `2x y0` | 2 | `r[x] ← r[y]`. Flags unchanged. |
| 0x3 | ADD  | `3x y0` | 2 | `r[x] += r[y]` mod 256. `zf ← truncated result==0`; `cf ← carry out (wide sum > 0xFF)`. |
| 0x4 | SUB  | `4x y0` | 2 | `r[x] -= r[y]` mod 256. `zf ← result==0`; `cf ← borrow` (`r[x] < r[y]` unsigned, pre-op). |
| 0x5 | INC  | `5x 00` | 2 | `r[x] += 1` mod 256. `zf ← result==0`. `cf` unchanged. |
| 0x6 | DEC  | `6x 00` | 2 | `r[x] -= 1` mod 256. `zf ← result==0`; `cf ← 1` iff `r[x]` was 0x00 before (wrap-around borrow), else cleared. |
| 0x7 | SHL  | `7x 00` | 2 | `r[x] <<= 1`. `cf ← old bit 7`; `zf ← result==0`. |
| 0x8 | JMP  | `8x nn` | 2 | `pc ← nn` (the `x` nibble is ignored). |
| 0x9 | JNZ  | `9x nn` | 2 taken / 1 not taken | if `zf == 0`: `pc ← nn`, else fall through. |
| 0xA | LD   | `Ax nn` | 3 | `r[x] ← mem[nn]`. |
| 0xB | ST   | `Bx nn` | 3 | `mem[nn] ← r[x]`. |
| 0xD | HLT  | `Dx 00` | 1 | `halted ← 1`. |

Opcodes 0x0, 0xC, 0xE, 0xF are undefined.

Note the deliberate traps versus LAB-8: operands live in NIBBLES, SUB's
borrow rule differs from DEC's, JNZ has asymmetric cycle costs, and HLT
costs 1. Spec-literal implementation wins; pattern-matching from memory of
LAB-8 loses.

## Errors

`step()` reports `StepError::UnknownOpcode` (undefined opcode; costs 1 cycle,
pc still advances past both bytes) or `StepError::BadRegister` (reserved
register field; costs 2 cycles, pc already advanced past both bytes). In both
cases the machine halts deterministically and no architectural state changes.

## Required interface

```cpp
namespace minicore {
enum class StepError { None, UnknownOpcode, BadRegister };
struct StepResult {
    uint32_t cycles = 0;
    uint16_t pc = 0;     // pc AFTER the step
    StepError error = StepError::None;
};
struct Cpu { /* state above */ };
// Cpu::load(std::span<const uint8_t>) copies the program to address 0 and
// resets all state. Cpu::step() executes ONE instruction.
// Cpu::run(max_cycles) steps until halt/error/budget and returns spent cycles.
}
```

## Definition of done

```bash
cmake --build build && ctest --test-dir build -R ch02_99 --output-on-failure
```

All `minicore.*` tests green. The hidden manifest additionally runs the
binary with test-name filters — partial implementations fail those cases.
