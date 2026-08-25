# 90 — Debugging: Two Bugs Loose in the Bus and Cartridge Layer

A "cleanup pass" touched the bus code and the iNES parser. Two independent
bugs slipped in. Your job is the full debugging loop:

```text
bug                    one sentence
root cause             the exact line and why it is wrong
first divergence       smallest program/trace where behavior differs
fix                    the patch (diff)
regression test        a test that fails before, passes after
```

Write your findings to `bug-report.md` in this directory.

## Symptom A — backgrounds scroll on the wrong axis

Games built for vertical arrangement (Super Mario Bros.-style side
scrolling) show their nametables stacked the wrong way. Every ROM with a
clean header is affected, so suspicion falls on the header decoder, not on
any one game.

Start here: `Header::parse` in `nesdbg.hpp`.

## Symptom B — DMA costs are exactly right... suspiciously exactly

A timing test suite reports every OAM DMA as 513 cycles. Half of them
should be 514 — the alignment get-cycle depends on the CPU cycle parity
when the $4014 write lands.

Start here: `trigger_oam_dma` in `nesdbg.hpp`.

## Hints

- The regression suite in `main.cpp` (`TEST(regression, ...)`) runs RED
  until both bugs are fixed.
- Fix the decode rule itself — never special-case callers.
