# 90 — Debugging: Two Bugs Loose in the Accuracy Layer

Someone "simplified" `cpu.hpp` and introduced **two independent bugs**.
Your job is the full debugging loop, not just a fix:

```text
bug                    one sentence
root cause             the exact line and why it is wrong
first divergence       smallest program/trace where behavior differs
fix                    the patch (diff)
regression test        a test that fails before, passes after
```

Write your findings to `bug-report.md` in this directory.

## Symptom A — INC is one cycle short and snooping devices miss a write

A memory-test ROM that watches the bus reports only ONE write for every
read-modify-write instruction, and traces log `cyc=4` where the reference
logs `cyc=5` for `INC $40`. Stores and loads behave, which is why nobody
noticed for weeks.

Start here: `rmw()` in `cpu.hpp`.

## Symptom B — NMI storm while the line is held

An NMI-driven scroll routine services its handler over and over as long as
the vblank line stays asserted — the stack churns and the counter proves
it. A correct 6502 latches ONE request per quiet->high transition; holding
the line stacks nothing.

Start here: the interrupt poll at the top of `step()`.

## Hints

- The regression suite in `main.cpp` (`TEST(regression, ...)`) encodes the
  correct hardware behavior. It runs RED until you fix both bugs.
- Fix nothing by special-casing call sites — repair `rmw()` and the poll.
- After fixing, re-run your FULL ch19 suite; the bugs can mask each other.
