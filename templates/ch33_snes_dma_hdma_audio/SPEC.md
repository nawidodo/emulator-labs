# SPEC — SNES DMA "Transfer Mode X" (fictional variant)

Status: NORMATIVE for `99_coding_test`. If code and spec disagree, the
spec wins.

## Overview

Mode X is a hypothetical ninth SNES DMA transfer pattern, defined as a
variant of the existing mode family. It behaves like an ordinary channel
in every way EXCEPT the three points listed below; everything else
(unit count semantics, bank:addr flattening, early stop when the A
address walks off mapped memory) matches exercise `01_dma`.

## Normative definition

1. **Units per transfer**: 4.
2. **B-bus register offsets** within one transfer (relative to BBADx):
   `+0, +1, +2, +1`. The third unit reaching +2 and the fourth folding
   back to +1 is the defining twist ("alternating pair with a peek").
3. **A-bus step**: ALWAYS increment by 1 after each byte. Control bits
   4-3 are IGNORED — even a "fixed" or "decrement" encoding produces the
   same upward walk. This mirrors how real modes 6/7 override bits 4-3,
   just in the opposite direction.

The pattern repeats every 4 bytes for transfers longer than one unit.

## Worked example

Channel state:

```text
control = $00      (bits 4-3 would say "increment"; irrelevant anyway)
b_reg   = $02      -> base register $2102
a_addr  = $1000
a_bank  = $00
unit_count = 9
```

Full expected bus sequence, byte by byte:

```text
i   b_addr   a_addr
0   $2102    $001000
1   $2103    $001001
2   $2104    $001002     <- +2 unit
3   $2103    $001003     <- folds back to +1
4   $2102    $001004     <- pattern restarts
5   $2103    $001005
6   $2104    $001006
7   $2103    $001007
8   $2102    $001008     <- partial unit: first byte only
```

## Additional required behaviours

- **Control-bit immunity**: with `control = $18` (fixed/decrement-ish
  encodings), the sequence is IDENTICAL to the table above. A step of +1
  per byte always.
- **Pattern periodicity**: `unit_count = 12` yields offsets
  `0,1,2,1,0,1,2,1,0,1,2,1`.
- **Bank addressing**: A address is `(bank << 16) | addr`, flattened;
  a transfer whose next flat address exceeds the provided memory image
  stops there (partial sequence), like `01_dma`.

## Interface to implement

`99_coding_test/coding.hpp`, namespace `snesdma::variant`:

```cpp
uint8_t unit_b_offset_x(int unit);                    // unit < 4
std::vector<TransferStep> run_mode_x(const Channel& ch,
                                     std::span<const uint8_t> a_bus);
```
