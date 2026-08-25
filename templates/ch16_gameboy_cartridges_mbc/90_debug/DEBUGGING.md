# Debugging drill — three seeded mapper defects

The skeleton of `cart_debug.hpp` contains excerpts of all three major
mappers, each carrying exactly one seeded defect. Each defect produces
plausible-looking bus traffic — the way real mapper bugs hide for weeks
in emulator projects.

| # | Defect | Symptom | Failing test |
|---|--------|---------|--------------|
| 1 | MBC1 `bank1` masked with 0x3F instead of 0x1F | banks above 31 select wrong physical banks; e.g. writing $21 selects bank 33 instead of bank 1 on a 64-bank cart | `debug_mbc1.*` |
| 2 | MBC3 latch handshake inverted (01-then-00 latches) | timer reads never freeze when a game follows the documented 00-then-01 procedure, but DO freeze under the inverted order | `debug_mbc3.*` |
| 3 | MBC5 $3000-$3FFF write ignored | 8 MiB-class games (bank selects >= 256) wrap into the first 4 MiB; later levels load earlier levels' graphics | `debug_mbc5.*` |

## Method

1. Run `ch16_90_debug_tests` and pick ONE failing suite.
2. Trace the value flow by hand for that test's inputs.
3. Fix, re-run, then write `bug-report.md`:

```text
bug:
root cause:
first divergence:   (exact input where stub and truth part ways)
fix:
regression test:    (name of the TEST you would add to prevent a relapse)
```

Repeat until all tests pass. Do not fix all three blind — the point is
the isolation workflow.
