# CHALLENGE — LAB-8X: subroutines, CALL/RET and a stack

The base LAB-8 has no subroutines: a "helper" must be inlined by hand. Real
ISAs solve this with a **stack** and two instructions. Extend the core:

## New machine state

```cpp
uint8_t sp = 0xFF;  // stack pointer: index of the next FREE stack byte
```

The stack lives at the TOP of the same 256-byte RAM and grows DOWNWARD
(exactly like the main-memory stacks of 6502, Z80 and x86):

- `push v`: `ram[sp] ← v; sp ← sp - 1`
- `pop`: `sp ← sp + 1; return ram[sp]` (the exact mirror)
- `sp == 0xFF` means empty. Popping an empty stack is a hard
  `StackUnderflow`; pushing when `sp == 0x00` would wrap into program space
  and is a hard `StackOverflow`. Both halt the machine deterministically,
  like every other step error.

## New instructions

| Opcode | Mnemonic | Encoding | Len | Cycles | Semantics |
|-------:|----------|----------|:---:|:------:|-----------|
| `0x80` | CALL | `80 aa` | 2 | 6 | push pc (return address — fetch() already advanced past the CALL), then `pc ← aa`. On push failure: halt with the error, do not jump. |
| `0x90` | RET | `90` | 1 | 6 | `pc ← pop()`. On underflow: halt with the error. |

CALL/RET do not touch flags. Note what the return address IS: fetch()
advanced pc past both instruction bytes before execute ran, so pushing c.pc
pushes exactly the right resume point — no +1 corrections anywhere.

## Acceptance criteria

Implemented in `cpu.hpp` (TODO blocks 1–4), pinned by `main.cpp`:

1. `push`/`pop` are exact mirrors; LIFO order preserved across multiple
   values.
2. CALL pushes the correct return address and jumps to the callee.
3. RET resumes at the instruction after the CALL.
4. Two-deep nesting returns through both frames in order.
5. A subroutine callable from two different call sites resumes correctly at
   each.
6. StackUnderflow on bare RET; StackOverflow on unbounded recursion (a
   self-calling program must halt with the error inside a sane cycle budget).
7. Flags survive CALL/RET untouched.

Run: build the exercise tests (`ch02_91_challenge_tests`) until green.
