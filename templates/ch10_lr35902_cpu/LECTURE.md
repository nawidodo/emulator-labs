# Chapter 10 — LR35902 (SM83) CPU Architecture

The Game Boy's CPU is a Sharp LR35902 (commonly called SM83): an Intel 8080
descendant mixed with Z80 ideas, stripped of most fancy addressing modes and
IX/IY, with Game Boy-specific power features added. Everything you build in
this phase sits on top of this core.

## Programmer's model

| Register | Width | Notes |
|---|---|---|
| A | 8 | accumulator |
| F | 8 | flags only; upper 4 bits meaningful, low nibble reads 0 |
| B, C, D, E, H, L | 8 | general purpose; BC/DE/HL usable as 16-bit pairs |
| SP | 16 | stack pointer (call/ret, push/pop) |
| PC | 16 | program counter |

Pairs are stored big-endian-in-name-only: `BC` means `B<<8 | C`. `AF` packs
flags into the high byte of F... careful: `A` is the *high* byte, `F` the low
byte, so `AF = A<<8 | F`.

## Flags (F)

| Bit | Mask | Name | Meaning |
|---|---|---|---|
| 7 | 0x80 | Z | set when result == 0 |
| 6 | 0x40 | N | set when last op was a subtraction |
| 5 | 0x20 | H | carry between bits 3 and 4 (BCD half-carry) |
| 4 | 0x10 | C | carry between bits 7 and 8 |

The low nibble of F reads as 0 and writes are ignored. H exists because DAA
needs it; it is *the* flag people get wrong. Half-carry rules:

- `ADD`: `(a & 0xF) + (b & 0xF) > 0xF`
- `SUB/CP`: `(a & 0xF) - (b & 0xF) < 0`
- `ADD HL,rr`: `(hl & 0xFFF) + (rr & 0xFFF) > 0xFFF`

## Instruction encoding

The SM83 opcode space decomposes cleanly into three bit fields. Every unprefixed
opcode is `xxyyzzzz` with:

- `x = op >> 6`  (block selector)
- `y = (op >> 3) & 7`
- `z = op & 7`

| Block | Meaning |
|---|---|
| x=0 | misc: 16-bit load/ALU on pairs, INC/DEC, rotates, DAA, JR |
| x=1 | `LD r[y], r[z]`; `z==6` means `(HL)`; `y==6 && z==6` is HALT |
| x=2 | ALU: `<add,adc,sub,sbc,and,xor,or,cp> A, r[z]` |
| x=3 | conditional jumps, stack ops, immediate-operand ALU, I/O loads |

Register index order: `B C D E H L (HL) A`.

### CB prefix

Opcode `CB` redirects the next byte to a second page:

- `y<4`: rotate/shift `r[z]` (RLC RRC RL RR SLA SRA SWAP SRL)
- `y==4`: `BIT y', r[z]` where `y' = op >> 5`
- `y==5`: `RES`
- `y==6/7`: `SET`

`(HL)` operands cost extra machine cycles because every memory access is its
own M-cycle. This is why the metadata table carries two cycle counts.

## Timing model

One M-cycle = 4 T-cycles. An instruction costs 4 T-cycles per memory access,
plus internal-delay M-cycles where hardware burns a cycle doing nothing
(`JR` taken, 16-bit loads, ...). We model timing with explicit
`cycles` / `cycles_alt` columns in the metadata instead of recomputing it.

## Exercises

1. `01_cpu_state` — registers, flag packing, pair views.
2. `02_opcode_meta` — `{name, bytes, cycles}` metadata generated from the
   encoding structure, base page + CB page.
3. `03_ld_alu` — decoder skeleton plus LD/ALU execution; the reference
   dispatch is driven by the metadata table.
4. `90_debug` — three seeded bugs in a working core; find and fix them.
5. `91_challenge` — run course-original CPU smoke programs against golden
   dumps/traces.
6. `99_coding_test` — implement an unseen opcode family (the LDH block)
   from a written specification.

## References

- Pan Docs, CPU section: https://gbdev.io/pandocs/CPU.html
- Pan Docs opcode map: https://gbdev.io/pandocs/Opcodes
