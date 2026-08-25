# Chapter 31 — The 65C816 CPU

The SNES main processor is the Ricoh 5A22, a WDC W65C816 core running
at ~3.58 MHz (2.68 MHz when accessing slow regions). It looks like a
6502 until you switch it on properly — then it becomes a 16-bit CPU
with a 24-bit address space, bank registers and runtime-switchable
operand widths.

## Study

```text
8/16-bit accumulator
8/16-bit index mode
bank registers
direct page
24-bit addressing
emulation/native modes
```

### Widths are runtime state

The status flag **M** (P bit 5) selects the accumulator width; flag
**X** (P bit 4) selects index width. `SEP #$20` sets M (A becomes
8-bit), `REP #$30` clears both. Three consequences dominate real code:

1. **Immediate length changes.** `LDA #$1234` is a 3-byte instruction
   with A wide and a 2-byte instruction with A narrow. Disassemblers,
   tracers and PC arithmetic all depend on it.
2. **The hidden high bytes survive.** The accumulator is really C =
   B:A. An 8-bit load replaces only A; B keeps its old value. Same for
   XH/YH.
3. **Flags are per-width.** Z/N reflect the value at the *current*
   width: `$12FF` sets N when A is 16-bit but clears it after an
   8-bit load of `$FF`? No — `$FF` as an 8-bit result sets N; the point
   is the SAME memory produces different flags depending on width.

### Emulation vs native

After reset E=1: the chip behaves like a 6502 — M and X forced set,
XH/YH cleared, SP page-locked to $01. `XCE` exchanges carry with E.
Leaving emulation mode does not widen anything by itself: REP/SEP do
that. Entering emulation clears XH/YH immediately.

### Bank registers

The 24-bit space is reached through registers:

- **K** (program bank) — where code executes from; changed by JML/JSL.
- **DB** (data bank) — default bank for absolute addressing; changed
  by `PLB`, preserved across JML. K != DB is a classic confusion.
- **D** (direct page) — base register for all `dp` addressing; the
  low byte of D participates in page-cross penalties.

### Addressing subset used in this chapter

| mode | bank | notes |
|---|---|---|
| dp | $00 | (D + off [+ index]) & $FFFF |
| abs | DB | wraps inside the bank when indexed |
| long | operand | ignores DB entirely |
| sr | $00 | SP + offset |

Cycle penalties: +1 when an INDEX addition crosses a page boundary;
dp indirect modes charge their own extra cycles (see SPEC.md in the
coding test).

### Solution note (curriculum)

The reference executor keeps operand width EXPLICIT at every execution
site: each LDA/STA consults the live M/X flags instead of assuming a
register size. That is what makes SEP/REP testable — the same opcode
changes length, timing and flags depending on history.

## Exercises

- `01_widths` — P packing, SEP/REP clamping in emulation, XCE side effects.
- `02_addressing` — effective-address math for dp/abs/long/sr with
  page-cross detection.
- `03_execute` — the executor itself plus runner CLI, trace format and
  a disassembler (`insn_len`, `mnemonic`).
- `90_debug` — a seeded width-misread bug; write a bug report.
- `91_challenge` — two-bank program crossing banks with mixed widths.
- `99_coding_test` — implement LDA (dp),Y from SPEC.md alone.

## References

- WDC W65C816S datasheet (the "13-bit" register diagrams).
- Anomie's SNES tracing docs, 65C816 section:
  https://problemkaputt.de/fullsnes.htm#snes65c816cpu
- E. Gower's 65816 reference: https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf
