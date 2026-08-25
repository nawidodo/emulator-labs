# Debugging Exercise — ch07: three seeded 8080 core bugs

A colleague's arithmetic-core branch "passes most programs" but three subtle
hardware-contract violations slipped in. Your job: find all three, fix them,
and file the report.

## The contract being violated

| # | Site | Hardware truth (8080) |
|---|---|---|
| 1 | `DCR` flag handling | **CY is PRESERVED** across INR/DCR. Only the ALU group, STC, CMC and DAD touch CY. |
| 2 | `CMP` result handling | CMP performs a subtraction for FLAGS ONLY — the accumulator is never modified. |
| 3 | `LDA` address operand fetch | Address bytes are fetched LOW byte first (8080 is little-endian); high-byte-first reads land at a byte-swapped address. |

## Symptoms you will reproduce

1. Countdown loops that combine `DCR` with carry-based decisions diverge as
   soon as the counter underflows (`ch07_90_debug dcr.*`).
2. Any program using `CPI` loses its accumulator value on first compare —
   loop counters evaporate (`cmp.leaves_accumulator_untouched`).
3. Programs loading data tables via `LDA <addr>` read zeros or garbage;
   hexdump shows the load landing near `0x00<hi>` instead of `<hi><lo>`
   (`lda.address_operand_is_low_byte_first`).

## Workflow (trace-first, curriculum §54)

```bash
ctest --test-dir build -R ch07_90_debug --output-on-failure
```

Each failing assertion names the exact architectural expectation. For every
bug, produce `bug-report.md` in this directory:

```text
bug:              <one line>
root cause:       <mechanism, not symptom>
first divergence: <instruction + register/flag state where good != bad>
fix:              <what changed>
regression test:  <the pinning test above that now guards it>
```

## Done means

- `ch07_90_debug` fully green in your skeleton tree.
- `bug-report.md` written with all five fields per bug.
- The hidden coding test re-checks these contracts from outside
  (`make grade GRADE_TARGETS=ch07_i8080_architecture`).
