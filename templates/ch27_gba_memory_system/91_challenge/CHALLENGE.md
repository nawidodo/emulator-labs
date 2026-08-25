# Challenge — ch27: Access-Sequence Timing Calculator

Curriculum goal: "Model access width and wait-state behavior." The
calculator reproduces the reference bus's cycle accounting for arbitrary
access sequences using only the region cost tables.

## Task

`timing_calc.hpp` contains two @LABS tasks:

1. `burst_total(region, first_addr, count, width)` — exact total for
   `count` adjacent accesses; the first bills non-sequential (it follows a
   pipeline refill), the rest are sequential while adjacency holds.
2. `fastest_rom_chip(count, width)` — which of WS0/WS1/WS2 completes the
   burst soonest (ties to the lower chip).

## Acceptance criteria

All five challenge tests pass, and the calculator's report matches the
committed golden:

```bash
./ch27_calc_runner > out.txt
diff out.txt ../../tests/public/ch27_gba_memory_system/goldens/calc_report.txt
```

The hidden grader hashes the same report plus unseen burst totals.
