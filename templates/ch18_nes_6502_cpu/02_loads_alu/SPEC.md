# 02 — Loads, Stores and the ALU over a Dispatch Table

Addressing modes are done. This exercise adds **opcode semantics** as free
functions `void op_xxx(Cpu&, uint16_t addr)` and wires them into the
256-entry decode table, where each row is `{mode_fn, op_fn, base_cycles,
penalty}`.

## Tasks

1. **Loads/stores** — `LDA/LDX/LDY` set N/Z from the loaded value;
   `STA/STX/STY` never touch flags.
2. **Transfers/stack** — `TAX TAY TXA TYA TSX TXS` (only TXS skips N/Z) and
   `PHA/PHP/PLA/PLP`. The stacked P always carries B=1, U=1; PLP strips B.
3. **Logic** — `AND ORA EOR`, plus `BIT`: N/V copied from *memory operand*
   bits 7/6, Z from `A & m`, A untouched.
4. **Arithmetic** — `ADC/SBC` through one shared binary-adder helper:
   SBC is ADC of `~m`. V formula: `(~(a^m) & (a^sum)) & 0x80`.

## Acceptance

- All tests pass; cycle totals match the official counts (the table's
  `base` column documents them; step() derives totals from bus accesses).
- Adding an instruction means adding exactly one table row — no new switch
  arms anywhere.

The table already routes every load/store/logic/arithmetic opcode of the
official set. Flow control (branches, jumps, compares, INC/DEC, shifts,
JSR/RTS, flag ops) lands in `03_flow_stack`.
