# 01 — Addressing Modes

Implement the 6502 addressing modes as independent functions before touching
a single opcode. Contract for every mode:

```cpp
bool mode_xxx(Cpu& cpu, uint16_t& out);
// consume own operand bytes, write effective address to `out`,
// return true when an indexed access crossed a page boundary
```

## Tasks

1. `mode_imm` — operand byte is the address; advance `pc` past it.
2. `mode_zp`, `mode_zpx`, `mode_zpy` — zero page; indexed forms wrap in
   8 bits (`$80,$X=$90` → `$0010`).
3. `mode_abs`, `mode_absx`, `mode_absy` — 16-bit little-endian fetch;
   indexed forms report page crossings (`(base & 0xFF00) != (out & 0xFF00)`).
4. `mode_izx`, `mode_izy`, `mode_ind` — indirect modes with the two classic
   hardware quirks: pointer bytes wrap inside page zero, and a pointer at
   `$xxFF` reads its high byte from `$xx00` (JMP quirk).

`mode_imp` / `mode_acc` are provided (no operands).

## Acceptance

- All tests in this directory pass.
- Cycle counts in the tests match: modes bill one cycle per memory access.

## Why modes first

Chapter opcodes are `{mode_fn, op_fn}` table rows (see `02_loads_alu`).
Every hour spent making modes exactly right is repaid tenfold when ADC,
STA and ASL all share the same `(zp),Y`.
