# DEBUGGING — 90_debug: wrong background wins

## Symptom

The Mode 1 compositor "works" in isolation: transparency falls through, and
single-layer scenes render correctly. But any scene where two opaque
background layers overlap shows the WRONG layer on top — BG2/BG3 pixels
cover BG1, and the picture looks inside-out (as if layers were drawn
back-to-front instead of front-to-back). Priority bits seem to make it
worse: giving BG2 `priority=1` pushes it over BG1.

`ch32_90_compose_tests compose.bg1_outranks_lower_layers` fails with the
stub code; that is your reproduction.

## Reproduction

```bash
# from a generated skeleton tree
ctest --test-dir build -R ch32_90_compose_tests --output-on-failure
```

The failing assertion pins the exact pixel: three candidates
(BG3 P1 = color 9, BG2 P1 = color 7, BG1 P0 = color 5) must resolve to
BG1's candidate at index 2.

## Hint ladder (read only as far as you need)

1. The bug is entirely inside `compose()` in `compose.hpp`. The key formula
   `(layer * 2 + priority)` matches exercise 03's documented rule.
2. Check what the function does when a candidate is NOT better than the
   current best. What does `>=` mean here? Which end of the key range does
   `best_key` start at?
3. Write down the keys for `{BG3 P1, BG2 P1, BG1 P0}` by hand: they are 5,
   3 and 0. Which index should win, and which index does the loop return?
4. The fix is a one-token comparison change plus one initializer. Do not
   restructure the loop.

## Root cause (for your report)

The stub keeps the candidate with the LARGEST key (`key >= best_key`,
starting from 0), i.e. it applies the painter's algorithm in the wrong
direction: farthest layer wins. SNES backgrounds are not painter-sorted —
each pixel's winner is the candidate with the smallest documented key.

## Deliverable

Write `bug-report.md` next to this exercise's sources with exactly five
sections:

```
bug:            <one sentence>
root cause:     <what line/logic is wrong and why>
first divergence: <the smallest input where stub and truth differ>
fix:            <the diff, described or pasted>
regression test: <which TEST() now guards this, and why it fails on the stub>
```

Then rebuild: all `ch32_90_compose_tests` cases must pass.
