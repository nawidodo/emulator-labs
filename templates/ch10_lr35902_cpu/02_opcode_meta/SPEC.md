# SPEC ch10 / 02_opcode_meta

Build the static opcode metadata tables for the SM83 base page and the CB
page. The struct every consumer sees:

```cpp
struct Instruction {
    const char* name;
    uint8_t bytes;       // instruction length in bytes (1..3)
    uint8_t cycles;      // minimum T-cycle cost
    uint8_t cycles_alt;  // extra T-cycles when a branch condition is taken
};
const Instruction& opcode_info(uint8_t op);  // base page
const Instruction& cb_info(uint8_t op);      // CB page (bytes always 2)
```

## Why generated, not typed out

The encoding is algebraic (`x = op>>6`, `y=(op>>3)&7`, `z=op&7`), so the
reference solution *builds* the table from those fields:

- x=1 block: `"ld <ry>,<rz>"`, 1 byte, 4 cycles (+4 per (HL) operand).
- x=2 block: `"<alu><rz>"`, 1 byte, 4 cycles (8 via (HL)).
- Everything irregular lives in explicit row lists with Pan Docs timing.

Generating from structure means a typo in one row cannot silently disagree
with its neighbors, and adding an undocumented opcode later touches one
place.

## Timing ground rules

- 1 M-cycle = 4 T-cycles; each memory access costs one M-cycle.
- Conditional instructions carry both costs explicitly (`cycles` not-taken,
  `cycles + cycles_alt` taken). Examples: `JR cc,e` 8+4, `RET cc` 8+12,
  `JP cc,nn` 12+4, `CALL cc,nn` 12+12.
- `(HL)` variants pay an access M-cycle even when they do not write:
  `INC (HL)` 12, `BIT n,(HL)` 12, `RES/SET n,(HL)` 16.

## Acceptance

All tests GREEN in `main.cpp`, including full 256-entry coverage with sane
byte/cycle ranges and spot-checked conditional deltas.
