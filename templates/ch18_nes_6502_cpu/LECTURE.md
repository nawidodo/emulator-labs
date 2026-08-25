# Chapter 18 — 6502 CPU

The Nintendo Entertainment System's CPU is a Ricoh 2A03: a MOS 6502 core
running at ~1.79 MHz (NTSC) with the decimal-mode circuitry removed and the
APU glued on-die. Everything you learned about accumulator machines in the
CHIP-8080 phases applies, but the 6502 has its own personality: a small
register file, a dense opcode space organized by *column = addressing mode*,
and cycle counts that fall straight out of how many memory accesses an
instruction performs.

## Registers

```text
A   8-bit accumulator — arithmetic and logic destination
X   8-bit index register — counters, indexed addressing
Y   8-bit index register — second index, used by (zp),Y and abs,Y
SP  8-bit stack pointer, offsets into page 1 (stack = $0100–$01FF),
    grows downward, resets to $FD
PC  16-bit program counter
P   8-bit status register NV-BDIZC:
      N negative  (bit 7, copy of result bit 7)
      V overflow  (bit 6, signed arithmetic overflow)
      - unused    (bit 5, always reads as 1)
      B break     (bit 4, only exists on the stacked copy of P)
      D decimal   (bit 3, the 2A03 cannot use it — ignore for NES)
      I interrupt disable (bit 2)
      Z zero      (bit 1)
      C carry     (bit 0)
```

## Addressing modes FIRST

The single most important structural decision in a 6502 emulator: **addressing
modes and opcode semantics are orthogonal**. `LDA $1234,X` and `CMP $1234,X`
do completely different things to the same fetched address. If you bake
address computation into every opcode handler you will duplicate the same
page-cross logic 40 times and get half of them wrong.

Implement each mode as its own function:

```text
imm     operand is the next byte              LDA #$10
zp      addr = zeropage byte                  LDA $80
zp,X    addr = (zp + X) & $FF                 LDA $80,X   (wraps!)
zp,Y    addr = (zp + Y) & $FF                 LDX $80,Y
abs     addr = 16-bit little-endian           LDA $1234
abs,X   addr = abs + X                        LDA $1234,X
abs,Y   addr = abs + Y                        LDA $1234,Y
(zp,X)  ptr = (zp + X) & $FF; addr from ptr   LDA ($40,X)
(zp),Y  ptr = zp; addr = [ptr] + Y            LDA ($40),Y
ind     JMP-only; pointer dereference         JMP ($1234)
```

Three details every beginner emulator gets wrong:

1. **Zero-page wraparound.** `zp,X` sums in 8 bits: `$80 + $90 = $10`, never
   `$110`. Same for the `(zp,X)` pointer and the `(zp),Y` pointer fetch — the
   pointer's high byte comes from `(ptr + 1) & $FF`.
2. **Page-cross cycle penalty.** Indexed *read* instructions (`abs,X`,
   `abs,Y`, `(zp),Y`) take one extra cycle when the index pushes the effective
   address past a page boundary (`$20FF + 2 = $2101`). Stores and
   read-modify-writes pay the penalty *always*, because the 6502 always does
   the speculative access first.
3. **JMP ($xxFF) page-wrap quirk.** The indirect pointer never crosses a page:
   `JMP ($30FF)` reads its high byte from `$3000`, not `$3100`. This is a
   hardware property, not a bug to fix — games of the era relied on it.

## Cycle accounting

On the 6502, the official cycle count of an instruction equals the number of
memory accesses it performs, including the "wasted" ones:

```text
LDA $1234      4 cycles: opcode, operand lo, operand hi, read
LDA $1234,X    4 cycles (+1 if page crossed): speculative read is the dummy
STA $1234,X    5 cycles: penalty always paid
ASL $80        5 cycles: opcode, operand, read, DUMMY WRITE of old value, write
```

Model every `read()` and `write()` as one cycle and the totals fall out
naturally. Chapter 19 makes the remaining dummy accesses explicit.

## Instruction groups

Once modes exist, opcodes become one-liners:

```text
loads/stores    LDA LDX LDY STA STX STY
transfers       TAX TAY TXA TYA TSX TXS
stack           PHA PHP PLA PLP (and JSR/RTS in the flow group)
logic           AND ORA EOR BIT
arithmetic      ADC SBC          (binary only; the NES has no decimal mode)
compares        CMP CPX CPY
inc/dec         INC DEC INX INY DEX DEY
shifts          ASL LSR ROL ROR  (accumulator and memory forms)
flow            JMP (abs and ind), 8 conditional branches, JSR/RTS/RTI/BRK
flags           CLC SEC CLI SEI CLV CLD SED, NOP
```

Flag rules worth memorizing:

- `ADC`: `C = sum > $FF`; `V` set when operands share a sign but the result
  differs: `(~(a^m) & (a^result)) & 0x80`.
- `SBC`: identical hardware path with `m = ~operand` and inverted carry-in.
- `CMP` is a subtract that throws the result away and keeps only C/Z/N.
- `BIT` copies bit 7 → N and bit 6 → V *of the memory operand* (not the
  result), and sets Z from `A & m`.

## Decoupling with a dispatch table

The reference solution uses a 256-entry table:

```cpp
struct Entry {
    ModeFn  mode;      // computes effective address, reports page cross
    OpFn    op;        // applies semantics to that address
    uint8_t base;      // documented base cycles
    Penalty penalty;   // None | OnCross | Always (indexed accesses only)
};
```

`step()` fetches the opcode, runs the mode, bills the penalty, calls the op.
Adding an instruction = adding one table row. This is also what makes the
chapter 19 accuracy work (dummy reads, unofficial opcodes) a matter of
editing rows instead of rewriting a giant switch.

## Study material

- NESdev wiki, "CPU basics" and the instruction/opcode matrix:
  https://www.nesdev.org/obelisk/
- 6502.org tutorials (addressing modes walk-through):
  https://www.6502.org/tutorials/6502opcodes.html
