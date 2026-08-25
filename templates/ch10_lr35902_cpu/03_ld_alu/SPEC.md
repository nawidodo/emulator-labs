# SPEC ch10 / 03_ld_alu

Decoder skeleton plus execution of the base-page LD/ALU subset.

## Implemented opcode set (this chapter)

- `LD r,r'`, `LD r,n`, `LD rr,nn`, `LD (HL),r/n`
- `LD A,(rr)`, `LD (rr),A`, LDI/LDD forms, `LD SP,HL`, `JP HL`
- `INC/DEC r`, `INC/DEC (HL)`, `INC/DEC rr`, `ADD HL,rr`
- ALU block: `ADD ADC SUB SBC AND XOR OR CP` with register, `(HL)`, and
  immediate operands
- Control flow: `JP nn`, `JP cc,nn`, `JR e`, `JR cc,e`, `HALT` (simple:
  sets `halted`; wake semantics arrive in Chapter 13 via interrupts)

Everything else traps (`Cpu::trap = true`) so silent no-ops can never hide a
missing implementation. The decoder exposes an `extra_exec` hook so later
chapters and coding tests plug in additional opcode families without editing
this file.

## Timing

Cycle counts come from the Chapter 10.2 metadata tables. Conditional jumps
pay `cycles_alt` only when taken — the skeleton must reproduce
`JR e`=12, `JR cc,e`=8/12, `JP nn`=16, `JP cc,nn`=12/16 exactly.

## Runner

`ch10_03_ld_alu_runner --rom P --headless [--cycles N] [--trace FILE]
[--dump FILE] [--hash-frame FILE]`. Fixture images are assembled at
`ORG $0100`. Trace lines (after each instruction):

```
pc=0100 op=21 af=01B0 bc=0013 de=00D8 hl=014D sp=FFFE cyc=12
```

`--hash-frame` writes the canonical final-state dump line; in this CPU-only
phase that *is* the "frame".
