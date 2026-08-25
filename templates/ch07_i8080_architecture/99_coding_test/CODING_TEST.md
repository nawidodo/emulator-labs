# Chapter 7 Coding Test — Ten Unseen Instructions

The grader builds this exercise and runs `i8080_coding_tests` once per
instruction with a labstest filter (`hidden.stax`, `hidden.ldax`, ...).
Every case must exit 0. Implement each unit in `extra_ops.hpp` exactly to
the hardware spec below — the hidden edge cases probe the corners.

## Spec table

| Mnemonic | Encoding | Semantics | Flags touched | T-states |
|---|---|---|---|---|
| `STAX B/D` | `02` / `12` | `(BC)` or `(DE)` ← A | none | 7 |
| `LDAX B/D` | `0A` / `1A` | A ← `(BC)` or `(DE)` | none | 7 |
| `INX rp` | `03/13/23/33` | pair ← pair + 1 (16-bit) | **none** | 5 |
| `DCX rp` | `0B/1B/2B/3B` | pair ← pair − 1 (16-bit) | **none** | 5 |
| `DAD rp` | `09/19/29/39` | HL ← HL + pair (16-bit) | CY only (carry out of bit 15) | 10 |
| `DAA` | `27` | decimal adjust after add | CY, AC, S, Z, P | 4 |
| `RLC` | `07` | A rotate left; bit7 → CY → bit0 | CY only | 4 |
| `RRC` | `0F` | A rotate right; bit0 → CY → bit7 | CY only | 4 |
| `RAL` | `17` | rotate left through carry (9-bit) | CY only | 4 |
| `RAR` | `1F` | rotate right through carry (9-bit) | CY only | 4 |

## DAA exact algorithm (8080)

```
correction = 0
if AC or (A & 0x0F) > 9:      correction |= 0x06
if old_CY or (A >> 4) > 9:    correction |= 0x60 ; new_CY = true
A += correction
S/Z/P from adjusted A; AC = low-nibble fix happened; CY never cleared
```

Note both comparisons use the ORIGINAL accumulator value.

## Traps the hidden cases check

- `INX/DCX` must not modify ANY flag (implement as one 16-bit add, not
  two nibble ops).
- `DAD` sets carry from bit 15 of the 16-bit sum, nothing else.
- Rotates affect CY only — never S/Z/P.
- `STAX/LDAX` use BC/DE indirection, NOT HL.
