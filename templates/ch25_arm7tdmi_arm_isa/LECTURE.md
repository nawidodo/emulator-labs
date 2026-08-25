# Chapter 25 — ARM7TDMI ARM Instruction Set

The GBA's CPU is an **ARM7TDMI**: a 32-bit ARMv4T core with a 3-stage
pipeline (fetch / decode / execute), 16 visible registers and a *conditional
execution* field on every instruction. Everything you build in chapters 25–28
sits on top of this ISA.

## Registers

| Register | Name     | Notes                                              |
|----------|----------|----------------------------------------------------|
| r0–r7    | —        | General purpose, unbanked                          |
| r8–r12   | —        | Banked for FIQ mode (fiq_r8..fiq_r12)              |
| r13      | SP       | Stack pointer, banked per privileged mode          |
| r14      | LR       | Link register, banked per privileged mode          |
| r15      | PC       | Program counter                                    |
| CPSR     | —        | Flags N Z C V, interrupt masks I F, mode bits M[4:0]|

**PC-reads-during-execution:** while an instruction executes, the pipeline has
fetched two further words, so r15 reads as `instruction_address + 8` in ARM
state (`+4` in Thumb). Branch targets written to r15 therefore need care:
`BL` stores `pc - 4` (the address of the instruction *after* the branch),
which is exactly the traditional return point.

## CPSR

```
31 30 29 28 | ... | 7 6 | 4:0
 N  Z  C  V  |     | I F | MODE
```

- **N** — negative: bit 31 of the result.
- **Z** — zero: result == 0.
- **C** — carry *out*. For adds it is the carry-out of bit 31. For subtracts
  ARM uses inverted borrow: `C = NOT borrow` (`SUB` sets C if no borrow
  occurred). Barrel-shifter results also produce a carry — see below.
- **V** — signed overflow: operands had the same sign and the result differs.

## Conditions (all 16)

Every ARM instruction has a 4-bit condition field; the instruction executes
only if the predicate holds. `{cond}=0b1111` (NV) never executes — it is the
encoding space for later extensions.

| Cond | Mnemonic | Meaning                | Test            |
|------|----------|------------------------|-----------------|
| 0000 | EQ       | equal                  | Z               |
| 0001 | NE       | not equal              | !Z              |
| 0010 | CS / HS  | carry set / unsigned ≥ | C               |
| 0011 | CC / LO  | carry clear / unsigned < | !C            |
| 0100 | MI       | minus / negative       | N               |
| 0101 | PL       | plus / non-negative    | !N              |
| 0110 | VS       | overflow               | V               |
| 0111 | VC       | no overflow            | !V              |
| 1000 | HI       | unsigned higher        | C && !Z         |
| 1001 | LS       | unsigned lower-or-same | !C \|\| Z       |
| 1010 | GE       | signed ≥               | N == V          |
| 1011 | LT       | signed <               | N != V          |
| 1100 | GT       | signed >               | !Z && (N == V)  |
| 1101 | LE       | signed ≤               | Z \|\| (N != V) |
| 1110 | AL       | always                 | —               |
| 1111 | NV       | never                  | —               |

## Barrel shifter

One operand of most data-processing instructions passes through a barrel
shifter before reaching the ALU. Shift types: LSL, LSR, ASR, ROR, plus **RRX**
(rotate right extended, one place through the C flag). The shifter produces
its own **carry-out**:

| Operation        | Result                    | Shifter carry-out           |
|------------------|---------------------------|-----------------------------|
| LSL #0           | Rm                        | unchanged                   |
| LSL #n (1..31)   | Rm << n                   | bit (32-n) of Rm            |
| LSL #32 (reg amt)| 0                         | bit 0                       |
| LSL #(reg) > 32  | 0                         | 0                           |
| LSR imm #0 (=32) | 0                         | bit 31                      |
| LSR #n (1..31)   | Rm >> n                   | bit (n-1)                   |
| ASR imm #0 (=32) | 0 or 0xFFFFFFFF (sign)    | bit 31 (sign)               |
| ASR #n (≥32)     | replicated sign           | sign bit                    |
| ROR imm #0       | **RRX**: (C<<31)\|(Rm>>1) | old bit 0                   |
| ROR #n           | rotate right (n mod 32)   | last bit rotated out (bit n-1 mod 32); ROR #32 leaves Rm, carry = bit 31 |

The immediate-operand form is itself a shifter use: an 8-bit value rotated
right by an even amount `2*rot`.

## Data processing

Opcode field (bits 24–21):

| Op | Name | Kind      | Writes Rd? |
|----|------|-----------|------------|
|0000| AND  | logical   | yes        |
|0001| EOR  | logical   | yes        |
|0010| SUB  | arith     | yes        |
|0011| RSB  | arith     | yes        |
|0100| ADD  | arith     | yes        |
|0101| ADC  | arith     | yes        |
|0110| SBC  | arith     | yes        |
|0111| RSC  | arith     | yes        |
|1000| TST  | logical   | no         |
|1001| TEQ  | logical   | no         |
|1010| CMP  | arith     | no         |
|1011| CMN  | arith     | no         |
|1100| ORR  | logical   | yes        |
|1101| MOV  | logical   | yes        |
|1110| BIC  | logical   | yes        |
|1111| MVN  | logical   | yes        |

**The key architectural subtlety:** when the S bit is set,

- *logical* operations (AND/EOR/TST/TEQ/ORR/MOV/BIC/MVN) write the **shifter
  carry-out** into C;
- *arithmetic* operations (SUB/RSB/ADD/ADC/SBC/RSC/CMP/CMN) write the **ALU
  carry** (carry/borrow of the addition) into C.

Two different carry sources, two different destinations. Our reference CPU
keeps them in separate variables and never lets one leak into the other —
this is the single most common ARM emulation bug.

With `Rd == 15` and S set, the instruction restores CPSR from SPSR (exception
return); we note it here and model it fully in chapter 26.

## Multiply

- `MUL Rd,Rm,Rs` — 32×32→32. Timing per GBATEK: 1N + 2S cycles, plus 1S when
  bits 8–30 of Rs are neither all-zero nor all-one.
- `MLA Rd,Rm,Rs,Rn` — multiply-accumulate: 1N + 3S (+1S same rule).
- `UMULL/SMULL/UMLAL/SMLAL` — 64-bit results: 1N + 3S (accumulate forms +1S).
  MUL may not use r15 and Rd != Rm.

## Single load/store

`LDR/STR` (word), `LDRB/STRB` (byte), `LDRH/STRH` (halfword, halfword-extension
space). Addressing: `Rn ± offset` with pre-index (`[Rn, #off]`),
pre-index-with-writeback (`[Rn, #off]!`) and post-index (`[Rn], #off`).
Unaligned word loads rotate the loaded data right by `(addr & 3) * 8` — the
byte at the aligned word boundary ends up in the MS byte. Loads cost 1N + 1S
(+1N extra refill when Rd == PC), stores 1N + 1S.

Load/store multiples (LDM/STM) transfer a register list, lowest register to
lowest address, costing `(n+1)S + 1N` for n registers. We cover them in the
challenge.

## Branches

- `B label`: `target = pc_of_branch + 8 + (sign_extend(imm24) << 2)`.
- `BL label`: as B, plus `LR = pc_of_branch + 4` (the "pc − 4" rule).
- Taken branch refills the pipeline: 2S + 1N cycles. Not-taken: 1S.
- `BX Rm`: jump to Rm; bit 0 selects Thumb state (chapter 26).

## Why this matters for GBA work

Games live and die on flags-and-shift code: division-by-multiply sequences,
`RRX`-based 64-bit shifts, conditional scheduling. An emulator whose carry
semantics drift breaks games in ways that look like random corruption. Get
the shifter carry / ALU carry split exactly right, and the rest of the ISA is
bookkeeping.
