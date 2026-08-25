# Challenge — ch23: reproduce the golden MMC3 interrupt log

Games time raster splits with the MMC3 IRQ counter. Your mapper must
reproduce the exact edge-by-edge schedule, including the nasty case of a
period rewritten mid-countdown (see 90_debug for the failure mode).

## Task

Run the IRQ lab runner over each fixture and reproduce these FNV-1a 64
hashes of the emitted interrupt log:

| Fixture | Scenario | FNV64 |
|---|---|---|
| `fixtures/irq_period.txt`  | period = latch+1, ack discipline | `PENDING` |
| `fixtures/irq_rewrite.txt` | mid-count latch rewrite          | `PENDING` |

Command shape (adjust binary path to your build tree):

```bash
./ch23_91_irq_runner --script templates/ch23_nes_mappers/91_challenge/fixtures/irq_period.txt \
    --trace /tmp/irq1.log
python3 tools/labs/hash_frame.py /tmp/irq1.log --fnv-only
```

The log format is pinned by the runner header: `IRQ@<edges>` lines whenever
the level-held line RISES, plus `edge= cnt= latch= en= irq=` snapshots.

## Acceptance criteria

- Both hashes match exactly.
- You can explain, per fixture, on which edge each `IRQ@n` fired and why
  the reload semantics put it there.
- Running your Chapter 23 MMC3 through `irq_rewrite.txt` produces the same
  log as the reference — if a mid-count `$C000` write truncates the current
  countdown, you have re-introduced the seeded bug.
