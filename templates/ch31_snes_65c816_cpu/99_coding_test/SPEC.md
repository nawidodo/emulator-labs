# SPEC — LDA (dp),Y, opcode $B7

## Signature

```cpp
int lda_dp_indirect_y(Cpu& c, const Mem& m, uint8_t dp_off,
                      bool* page_crossed);
```

Returns the BASE cycle count (5). When adding Y to the pointer moves it
into a different 256-byte page, `*page_crossed` is set true and the
CALLER charges one extra cycle.

## Behavior

1. **Pointer fetch.** The 16-bit pointer lives in BANK ZERO at address
   `(D + dp_off) & 0xFFFF` — never in DB, regardless of D's value.
   Low byte first.
2. **Index.** `Y` participates at its CURRENT width: when FX forces
   8-bit indexes, only `$00..$FF` of Y is used (`xy_mask`). The sum is
   `(ptr + Y) & 0xFFFF`; there is no bank spill.
3. **Page cross.** Crossed iff `(ptr & $FF00) != ((ptr + Y) & $FF00)`.
4. **Data fetch.** The effective address `sum` is interpreted inside
   the **DB bank**: read `A`-width bytes starting at `(DB << 16) |
   sum`. With M set (8-bit A) read ONE byte and preserve the hidden
   high byte B; otherwise read two.
5. **Flags.** Z/N updated from the loaded value at the current
   accumulator width. All other flags untouched.

## Worked examples

| D | dp | ptr | DB | Y (width) | data addr | cycles | crossed |
|---|----|-----|----|-----------|-----------|--------|---------|
| $0200 | $10 | word@$0210 = $1234 | $7E | $0002 (16) | $7E:$1236 | 5 | no |
| $0000 | $00 | word@$0000 = $20FF | any | $0002 (16) | ?: $2101 | 5(+1 by caller) | yes |
| $0200 | $05 | word@$0205 = $3000 | $00 | $0010 (16) | $00:$3010 | 5 | no |
| $0200 | $02 | word@$0202 = $21F0 | any | $EF (8-bit Y) | ?: $22DF | 5(+1 by caller) | yes |

Note the last row: an 8-bit Y can still cross a page boundary — it is
the pointer's low byte position that decides, not the index width.

## Edge cases pinned by the public tests

- Data must come from the DB bank even when a different byte sits at
  the same offset in bank zero.
- `*page_crossed` is assigned, never OR-ed in.
- 8-bit load preserves hidden B and reads exactly one byte.
