# Chapter 7 — Intel 8080 Architecture

This is our first **real hardware CPU**. Everything from the CHIP-8 phase
(fetch/decode/execute, StepResult timing, trace-first debugging) now applies
to a chip that really shipped in 1974 and powered thousands of arcade boards.

## The programmer's model

```text
        15 8 7 0
       +---+---+
  SP   |   |   |   stack pointer (16-bit)
  PC   |   |   |   program counter
       +---+---+
  A  \ / F |   accumulator + flags ("PSW" when pushed as a pair)
  B  | C |     general pairs: BC, DE, HL
  D  | E |     HL doubles as a memory pointer (the "M" register)
  H  | L |
```

Eight-byte register file `A B C D E H L` plus two 16-bit registers. Any
three of B, D, H can pair up with the following byte into BC/DE/HL — the
opcode encoding literally stores "which pair" in two bits.

### Flags (the PSW byte)

Pushed by `PUSH PSW`, bit order MSB→LSB: **S Z 0 AC 0 P 1 CY**.

| Flag | Meaning | Exact rule |
|---|---|---|
| S | sign | copy of bit 7 of the result |
| Z | zero | result == 0x00 |
| AC | aux carry | carry out of **bit 3**, feeding DAA |
| P | parity | set when the result has an **even** number of 1 bits |
| CY | carry | carry/borrow out of bit 7 |

Two details people get wrong for years:

- **Subtraction runs through the adder.** `SUB v` is computed as
  `A + ~v + 1`. CY means *borrow* (carry-out clear), and AC comes from the
  same half-sum on the complemented operand. Implement it that way and every
  edge case falls out for free.
- **AND sets AC weirdly.** On the 8080, `ANA/ANI` clear CY but set
  `AC = OR(bit3 of A, bit3 of operand)` — not forced to 1 like the 8085,
  not cleared. Diagnostics that run right before `DAA` depend on this.
- `XRA/ORA` clear both CY and AC. `XRA A` is the canonical
  zero-accumulator-and-flags idiom you will see in every arcade ROM.
- `INR/DCR` never touch CY; they do compute AC (nibble overflow/borrow).

### Condition codes

Eight conditions select jump/call/return variants, encoded in three bits:

| Code | Bits | Meaning | Test |
|---|---|---|---|
| NZ | 000 | not zero | Z = 0 |
| Z  | 001 | zero | Z = 1 |
| NC | 010 | no carry | CY = 0 |
| C  | 011 | carry | CY = 1 |
| PO | 100 | parity odd | P = 0 |
| PE | 101 | parity even | P = 1 |
| P  | 110 | plus | S = 0 |
| M  | 111 | minus | S = 1 |

## Instruction groups for this chapter

Data movement and ALU only — control flow arrives in chapter 8:

- `MOV r,r'` (`01DDDSSS`) — 5 T-states; 7 when either side is memory at HL
- `MVI r,data` — immediate load, 7 T-states (10 to memory)
- `LXI rp,d16` — load a pair little-endian, 10 T-states
- `LDA addr / STA addr` — direct A↔memory, 13 T-states
- `INR/DCR r` — 5 T-states (10 through M)
- `ADD/ADC/SUB/SBB/ANA/XRA/ORA/CMP r` — 4 T-states (7 through M)
- same group with immediate operand (`ADI/SUI/...`, `11OOO110`) — 7 T-states

Note the quirk that confuses newcomers reading timing tables: register ALU
ops (4T) are *faster* than MOV (5T). The ALU sits next to the register file;
MOV pays an extra state for the decode path.

## Architecture of the reference solution

The solution separates each instruction into stages so flag bugs can't hide:

```text
operand read   -> fetch bytes / read registers or bus
ALU            -> pure value computation ({result, CY, AC} out)
flag calc      -> S/Z/P/AC/CY from the ALU result
write-back     -> commit to registers/bus (CMP skips this)
```

Keep your implementation structured the same way — chapter 8 reuses it and
chapter 9 wraps the whole core behind a machine bus.

## Timing is part of the contract

`step()` returns the exact T-state cost of the instruction it executed
(curriculum §56). Cycle counts are not decoration: Space Invaders' interrupt
timing, cassette loading, everything downstream depends on them. The runner
emits one canonical trace line per instruction:

```text
pc=0002 op=C6 af=2502 bc=0000 de=0000 hl=0000 sp=0000 cyc=14
```

Compare traces with `tools/labs/compare_trace.py expected.log actual.log`
and hunt the **first divergence**, not the last visible symptom (§54).

## References

- Intel 8080 Assembly Language Programming Manual (Rev. B) — instruction
  semantics and the original timing tables
- Intel 8080/8085 Assembly Language Programming family guide — PSW layout
