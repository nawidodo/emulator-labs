# 99 — Coding Test: Ten Unseen Opcode/Mode Combinations

The exercises built the official core but deliberately left ten
opcode/addressing-mode combinations out of the decode table. The table
pattern is established; your job is to slot in the last ten rows using ONLY
the spec table below (no peeking at other emulators).

Every row needs the right mode function, op function, base cycles and penalty
kind. Semantics all exist already — this is a decode + timing exercise.

## The ten combinations

| # | Instruction | Opcode | Bytes | Base | Penalty | Notes |
|---|---|---|---|---|---|---|
| 1 | ORA abs,X   | `$1D` | 3 | 4 | OnCross | |
| 2 | AND abs,Y   | `$39` | 3 | 4 | OnCross | |
| 3 | EOR (zp,X)  | `$41` | 2 | 6 | None    | pointer wraps in page zero |
| 4 | ADC zp,X    | `$75` | 2 | 4 | None    | |
| 5 | SBC abs,X   | `$FD` | 3 | 4 | OnCross | |
| 6 | CMP (zp),Y  | `$D1` | 2 | 5 | OnCross | |
| 7 | INC abs,X   | `$FE` | 3 | 7 | ALWAYS  | indexed RMW never skips it |
| 8 | DEC abs,X   | `$DE` | 3 | 7 | ALWAYS  | |
| 9 | ROR zp,X    | `$76` | 2 | 6 | None    | rotates through C |
| 10 | BIT zp     | `$24` | 2 | 3 | None    | N/V from operand bits |

## Acceptance

- All `unseen.*` tests pass with exact cycle counts.
- Hidden grading runs the same suite names against your generated skeleton.
