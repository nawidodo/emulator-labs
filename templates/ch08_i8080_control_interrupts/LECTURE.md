# Chapter 8 — 8080 Control Flow, Stack and Interrupts

Chapter 7 gave the 8080 a data path; this chapter gives it a spine:
subroutines, conditional execution, and hardware interrupts.

## The stack

The 8080 stack grows **downward**. PUSH pre-decrements: high byte to
SP−1, low byte to SP−2, SP −= 2 (11 T-states). POP reverses it (10
T-states). `PUSH PSW` stores A in the high byte and the packed flags in
the low byte — bit 1 always reads 1, bits 3 and 5 always read 0.
`POP PSW` is the only instruction that can restore CY and AC at once,
which makes it the standard save/restore idiom around flag-sensitive code.

## Control flow and its timing

| Instruction | Encoding | Taken | Not taken |
|---|---|---|---|
| `JMP addr` | C3 | 10T | — |
| `Jcc addr` | 11CCC010 | 10T | **7T** |
| `CALL addr` | CD | 17T | — |
| `Ccc addr` | 11CCC100 | 17T | **11T** |
| `RET` | C9 | 10T | — |
| `Rcc` | 11CCC000 | 11T | **5T** |
| `RST n` | 11NNN111 | 11T | — |
| `PCHL` / `SPHL` | E9 / F9 | 5T | — |

The asymmetries are not arbitrary: an untaken conditional jump still
fetched its address operand (7T), an untaken call fetched AND decoded it
(11T), while an untaken return has nothing more to fetch at all (5T).
Space Invaders' interrupt cadence depends on this exact accounting, so
getting these numbers wrong shows up as a drifting frame boundary.

Condition codes (bits 5-3 of the opcode): NZ Z NC C PO PE P M — the same
S/Z/CY/P/S flags from chapter 7.

## RST — the one-byte call

`RST n` pushes PC and jumps to `n*8`. The vector lives entirely inside
the opcode bits (no operand fetch), which is why it costs only 11T and
why hardware interrupts use RST opcodes as their entry points.

## Interrupts

The model:

```text
device raises INTR
      |
CPU finishes current instruction
      |
IFF set? --no--> instruction ignored, continue
      |yes
INTA cycle: device jams an opcode onto the bus (normally RST n)
      |
CPU: push(PC); PC = n*8; IFF <- 0; resume
```

- `EI` (FB) sets the interrupt flip-flop, `DI` (F3) clears it (4T each).
- Acknowledge **clears IFF**, so an interrupt handler runs with further
  interrupts masked until it executes EI itself.
- An accepted interrupt wakes a HALTed processor.
- Simplification vs silicon (documented, deterministic): our EI takes
  effect immediately instead of one instruction late.

## Timing is part of StepResult

Every handler returns its exact T-state cost through `step()`. The
debugging exercise seeds two classic bugs — swapped call timings and a
byte-swapped pop — and asks you to find them by TRACE comparison, not by
staring at code (curriculum §54). The coding test automates that skill:
given golden vs buggy traces, report the first divergence.

## References

- Intel 8080 Assembly Language Programming Manual — CALL/RET/RST timing
- Space Invaders hardware notes — INTA jam-opcode practice (used heavily
  in chapter 9)
