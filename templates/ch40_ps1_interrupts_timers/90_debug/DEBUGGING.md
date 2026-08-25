# DEBUGGING — ch40 / 90_debug: the disappearing interrupts

## Symptom

The system boots and single sources work, but as soon as a driver
acknowledges one interrupt while another is pending, the *other* interrupt
is silently gone. Concretely:

- Timer2 and the controller both latch into `I_STAT`.
- The driver acknowledges Timer2 (`I_STAT = 1 << 6` — a plain
  write-1-clears acknowledge).
- The controller interrupt never fires. Reading `I_STAT` shows `0x000`,
  not the expected `0x200`.

A second flavor of the same loss: acknowledging a source whose peripheral
line is **still asserted** (the device has not been serviced yet) makes the
bit vanish until the next event of that source — on real hardware the line
would re-latch immediately.

## Reproduce

```bash
# skeleton tree (buggy)
ctest --test-dir build --output-on-failure -R ch40_90_debug   # RED

# fixed reference
ctest --test-dir build-solutions --output-on-failure -R ch40_90_debug  # GREEN
```

Failing tests: `debug_ack.partial_ack_preserves_other_sources`,
`debug_ack.still_asserted_line_relocks`,
`debug_ack.write_one_clears_only_those_bits`.

## Hints (read progressively)

1. `ack()` receives the written halfword. What does psx-spx say a write to
   I_STAT does with 0 bits? With 1 bits?
2. Compare with the DMA DICR acknowledge in ch43 — same idiom.
3. After clearing acknowledged bits, what does the level model require for
   lines that are still held high?

## Expected bug report (`bug-report.md`)

- **Bug**: one sentence.
- **Root cause**: which line of `irq.hpp`, and why it is wrong.
- **First divergence**: the earliest test assertion that fails, and why
  that is the first *observable* difference.
- **Fix**: the corrected code.
- **Regression test**: name the test(s) above that fail before and pass
  after your fix; add one more if you found an angle they missed.
