# 02 — Dummy Accesses and the Unofficial Subset

The 6502's cycle counts are not a price list — they are a bus transcript.
Every cycle is a read or write (or an invisible internal tick), including
the ones that never touch a register. RecordingBus lets the tests watch
them.

## Tasks

1. **RMW double-write** — `rmw()` writes the OLD value back before the new
   one. That extra real write is the official 5th cycle of `INC $zp`.
2. **Speculative indexed reads** — indexed stores/RMW always read the
   un-fixed-up address `{base_high, sum_low}` (`$2000` when
   `STA $20FE,X` with X=2 targets `$2100`); plain indexed reads do it only on a page cross.
3. **Internal penalties** — zp,X/zp,Y/(zp,X) pointer arithmetic bills one
   cycle with NO observable access.
4. **Unofficial subset** — implied/immediate/memory NOP families with real
   timings; LAX, SAX; the RMW combos DCP/ISB/SLO/RLA chained through
   `rmw()`.

## Acceptance

- All `dummy.*` tests pass: dummy-write ordering visible in the bus log,
  speculative reads at the wrong-page address, exact unofficial-opcode
  cycle counts.
