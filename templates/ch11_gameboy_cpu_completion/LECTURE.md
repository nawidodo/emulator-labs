# Chapter 11 — Game Boy CPU Completion

Chapter 10 built the regular LD/ALU core. This chapter finishes the CPU:
the BCD adjust, the CB page, stack control flow, and the interrupt machine.

## DAA — the exact algorithm

DAA adjusts A after a binary ADD/SO that was meant to be BCD. It reads the
*current* Z/N/H/C and the value of A:

```text
if N == 0:                      # after addition
    if C or A > 0x99: A += 0x60; C = 1
    if H or (A & 0xF) > 9: A += 0x06
else:                           # after subtraction
    if C: A -= 0x60
    if H: A -= 0x06
Z = (A == 0); H = 0             # C keeps its new value
```

The half-carry path is where emulators go wrong. `0x45 + 0x38 = 0x7D` with
H=1 must become `0x83`; `0x40 - 0x25 = 0x1B` with N=1,H=1 must become
`0x15`. Test both directions plus the double-adjust (`+0x66`) case.

## Rotates vs shifts

| Op | bit0 source | C source | Notes |
|---|---|---|---|
| RLC | old bit 7 | old bit 7 | pure rotate |
| RL  | old C     | old bit 7 | through carry |
| RRC / RR | mirror of above | | |
| SLA | 0 | old bit 7 | arithmetic left |
| SRA | shifted right, bit 7 preserved | old bit 0 | sign-keeping |
| SRL | 0 | old bit 0 | logic right |
| SWAP | nibbles swap | 0 | |

Base-page forms (`RLCA`...) force **Z = 0** — their CB twins set Z from the
result. BIT leaves C untouched and pins N=0, H=1. RES/SET touch no flags.

## Stack control flow

PUSH costs 16 cycles (internal + two writes), POP 12. Conditional pairs carry
both prices explicitly:

| Instruction | not taken | taken |
|---|---|---|
| RET cc | 8 | 20 |
| CALL cc | 12 | 24 |
| JP cc,nn | 12 | 16 |
| JR cc,e | 8 | 12 |

POP AF must re-mask F's low nibble to 0. RST is a one-byte CALL to `$00`,
`$08`, ... `$38`.

## HALT, EI, DI and the interrupt machine

- **IF** (`$FF0F`) holds pending lines: VBlank, STAT, Timer, Serial, Joypad.
  **IE** (`$FFFF`) masks them. A line fires when `IF & IE != 0`.
- **HALT** sleeps until `(IE & IF) != 0`. With IME set the wake jumps into
  the ISR (20-cycle dispatch: push PC, vector, clear IF, IME←0). With IME
  clear it resumes on the next instruction without servicing.

### The HALT bug (documented; we implement the sane version)

On real hardware, when HALT executes while IME=0 **and** `(IE & IF) != 0`
the CPU fails to increment PC internally once: the byte after HALT is read
*twice*. If that byte is a multi-byte opcode with register-dependent
behavior (e.g. `LD HL,SP+e`), execution corrupts state. Emulators that need
Mooneye's `halt_bug` test must model this skip explicitly. Our core always
sleeps/wakes cleanly and documents the difference.

### EI delay

EI does not enable interrupts immediately: IME rises only **after the
following instruction retires**. `EI; HALT` therefore may sleep through an
interrupt on exact hardware timing. We count down a two-phase delay at the
instruction boundary. DI is immediate; RETI restores IME immediately.

## References

- Pan Docs: https://gbdev.io/pandocs/CPU.html , /Interrupts, /Halting
- Mooneye acceptance tests (optional, student-supplied ROMs):
  https://github.com/retrio/gb-test-roms
