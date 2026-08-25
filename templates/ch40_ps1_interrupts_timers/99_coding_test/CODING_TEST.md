# CODING_TEST — ch40: unseen configuration, exact IRQ delivery order

You get the chapter's verified interrupt controller and root counters as
**given infrastructure** (`irq.hpp`, `timers.hpp` in this directory). What
you implement is the pipeline that turns an unseen *configuration image*
into the exact order in which root-counter interrupts reach `I_STAT`.

## The task

Implement the three `TODO(n)` blocks in `runner_main.cpp`:

| TODO | Function        | What it must do |
|------|-----------------|-----------------|
| 1    | `load_config`   | Validate the `CTIM` magic, read `run_cycles` from word 1, collect `{timer, mode, target}` triples until the `0xFFFFFFFF` terminator; reject malformed images. |
| 2    | `apply_config`  | Program each entry's TARGET **before** its MODE — the MODE write resets the counter and starts the root counter. |
| 3    | `Rig::tick_cb`  | Sample the cycle's video/dot signals, tick all counters through the sink, and reschedule for the next cycle. |

## Fixture format (little-endian words)

```text
word 0      magic "CTIM" = 0x4D495443
word 1      run_cycles
word 2+3k+0 timer index (0..2), or 0xFFFFFFFF to end
word 2+3k+1 MODE value (psx-spx bit layout, bits 0-12)
word 2+3k+2 TARGET value
```

## Environment

Identical to the rest of ch40: one scheduler event per system cycle,
hblank period 200 (40 wide), vblank every 25 hblanks (400 wide),
dotclock pulse every 6 cycles. Delivery order within a single cycle is
timer 0, then 1, then 2.

## Output contract

- Every rising edge into `I_STAT` appends one line to the log:
  `irq=<hex> cyc=<n>` where `<hex>` is the timer's I_STAT bit mask and
  `<n>` the delivery cycle.
- `--trace FILE` writes the log; `--hash-frame FILE` writes a single line
  `fnv64=<16 uppercase hex>` computed with FNV-1a-64 over the log bytes.

The hidden grader runs your build against an UNSEEN configuration image and
compares both log hash values. Any deviation — wrong clock source decode,
missing reset-on-target, off-by-one target compare, lost reschedule —
changes at least one delivery cycle and fails the hash.

## Self-check before submitting

```bash
python3 tools/labs/generate.py --force --targets ch40_ps1_interrupts_timers/99_coding_test
cmake --build build -j && ./build/skels/ch40_ps1_interrupts_timers/99_coding_test/ch40_99_coding_test_runner \
  --rom tests/public/ch40_ps1_interrupts_timers/roms/coding0.bin --trace /tmp/c.log --hash-frame /tmp/c.txt
diff <(sed 's/^fnv64=//' /tmp/c.txt) <(echo "SEE tests/public provenance.md")
```
