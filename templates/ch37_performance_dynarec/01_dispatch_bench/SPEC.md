# rx8 — the toy RISC machine of ch37

rx8 is a deliberately small load/store RISC core. Every performance idea in
this chapter (dispatch cost, decode caching, basic blocks, IR, optimization,
invalidation) is exercised on it at a scale you can hold in your head.

## Architectural state

| state | shape |
|---|---|
| `r[8]` | eight 32-bit registers; **r0 is hardwired zero** (reads 0, writes discarded) |
| `mem` | 16384 bytes, byte-addressable, little-endian words on 4-aligned addresses |
| `pc` | byte address, always a multiple of 4 |
| `halted`, `fault` | sticky run-control flags |
| `out` | output port log (append-only `uint32_t` values) |
| `executed` | retired-instruction counter — **the performance proxy for this chapter** |

Grading in ch37 NEVER uses wall time. The metric is the number of executed
guest instructions (interpreter tier) or executed IR operations (pipeline
tier), counted deterministically by the machine itself.

## Instruction encoding

Every instruction is one 32-bit little-endian word:

```text
bits  31..24   opcode
      23..20   rd
      19..16   rs
      15..12   rt
      11..0    imm12   (raw field; meaning depends on opcode)
```

Immediate interpretation:

- `ADDI` / memory offsets: imm12 **sign-extended** to 32 bits.
- `BEQZ` / `BNEZ` / `JMP`: the target is an **absolute byte address**
  computed as `imm12 << 2` (range 0 .. 16380, covering all of memory).

## Opcode table

| op | mnemonic | format | semantics |
|----|----------|--------|-----------|
| 0x00 | `nop`   | —      | no effect |
| 0x01 | `mov`   | `rd, rs` | `r[rd] = r[rs]` |
| 0x02 | `add`   | `rd, rs, rt` | `r[rd] = r[rs] + r[rt]` (wrapping) |
| 0x03 | `addi`  | `rd, rs, imm` | `r[rd] = r[rs] + sext(imm12)` |
| 0x04 | `sub`   | `rd, rs, rt` | `r[rd] = r[rs] - r[rt]` (wrapping) |
| 0x05 | `and`   | `rd, rs, rt` | bitwise and |
| 0x06 | `or`    | `rd, rs, rt` | bitwise or |
| 0x07 | `xor`   | `rd, rs, rt` | bitwise xor |
| 0x08 | `shl`   | `rd, rs, rt` | `r[rd] = r[rs] << (r[rt] & 31)` |
| 0x09 | `shr`   | `rd, rs, rt` | logical shift right, `r[rt] & 31` |
| 0x0A | `lw`    | `rd, off(rs)` | `r[rd] = mem32[r[rs] + sext(off)]` |
| 0x0B | `sw`    | `rt, off(rs)` | `mem32[r[rs] + sext(off)] = r[rt]` |
| 0x0C | `beqz`  | `rs, label` | `if r[rs] == 0: pc = imm12 << 2` |
| 0x0D | `bnez`  | `rs, label` | `if r[rs] != 0: pc = imm12 << 2` |
| 0x0E | `jmp`   | `label` | `pc = imm12 << 2` |
| 0x0F | `out`   | `rd` | append `r[rd]` to the output log |
| 0x10 | `halt`  | — | stop retiring instructions |

Field note for `sw`: the assembler mnemonic is `sw src, off(base)`, and in
the encoded word the BASE register rides in the **rd** field while the
stored source rides in the **rs** field (mirroring `lw`, whose destination
is also in rd). IR lowering in exercise 03 preserves these positions.

Notes:

- Writes to r0 are discarded everywhere (including loads and fused ops).
- `lw`/`sw` with a misaligned or out-of-range address raise `fault`.
- An unknown opcode raises `fault`. `fault` and `halted` are sticky: once
  set, `step()` refuses to retire more instructions.
- Programs load at address 0. Bytes never written read as zero (`nop`).

## Observable state (what golden hashes cover)

The chapter's equivalence contract is deliberately emulator-grade: two
pipelines are "bit-exact" when their **output logs and final memory images**
agree — internal register values are NOT part of the observable dump. This
is what legally allows the optimizer to delete register-only computations
whose values can never reach `out` or `mem`.

Canonical dump text (hashed by runners via FNV-64):

```text
out <hex8> <hex8> ...
mem <hex4 addr>=<hex8> ...   (nonzero aligned words, ascending)
```

## Why r0 and absolute branch targets?

r0-as-zero removes a dedicated `li` opcode (constant loads become `addi
rd, r0, k`) and gives the ch37/04 optimizer natural folding candidates.
Absolute imm12 branch targets keep the control-flow analyzer in
ch37/03 trivially precise: every target is a full address, no ranges.
