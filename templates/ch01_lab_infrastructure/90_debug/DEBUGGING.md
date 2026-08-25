# DEBUGGING — 90_debug: the loader that lies

Two decoders in `decode.hpp` are broken on purpose. The suite runs RED out
of the box; your job is to turn it GREEN **by diagnosis**, not by guessing,
and to leave a written trail.

## Reproduce

```bash
LABS=ch01_lab_infrastructure/90_debug make skels
make build && ctest --test-dir build --output-on-failure -R ch01_90_debug
```

## Symptoms reported by "users"

1. *Cartridge headers fail magic checks.* A header that stores
   `0x12345678` little-endian reads back as `0x78563412`. Curiously, every
   16-bit field of the same header looks perfectly normal.
2. *Big-endian fields are garbled.* CHIP-8-style opcodes come back wrong,
   and the values look like they were assembled from the *middle* of the
   buffer. A reviewer also flagged that this decoder touches one byte past
   the logical two-byte field — mid-buffer that is silent corruption, at a
   buffer end it would be an out-of-bounds read.

## Method (hint escalation — stop as soon as you can proceed)

- Hint 0 (concept): multi-byte loads are order-sensitive; check which byte
  lands in which significance lane.
- Hint 1 (structure): write each byte and its destination bits on paper
  for a known input (`78 56 34 12` -> `0x12345678`).
- Hint 2 (algorithm): compare against the correct `read_le16` sitting
  right above the first bug; the same widening discipline applies.
- Hint 3 (tools): run under lldb or with sanitizers
  (`make sanitize`) and watch which address is read.

## Deliverable — `bug-report.md` in this directory

For EACH of the two bugs:

```
## Bug N: <short name>
bug:         <what misbehaves, one sentence>
root cause:  <the exact faulty expression/line>
first divergence: <smallest input + expected vs actual>
fix:         <the corrected code>
regression test: <which TEST you added/strengthened and why it catches it>
```

Add at least one regression test per bug to `main.cpp` in YOUR skeleton
copy (do not edit the template). The gate for this exercise is the green
suite plus the completed `bug-report.md`.
