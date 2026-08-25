# Debugging drill — two seeded APU defects

The skeleton of `audio_debug.hpp` renders a sweep+envelope voice that
sounds *almost* right. Both defects live in code paths students rarely
step through, which is exactly how real APU bugs hide.

| # | Defect | Symptom | Failing test |
|---|--------|---------|--------------|
| 1 | Envelope off-by-one: `envelopeTick` burns one extra idle tick after every reload before the volume moves (effective period+1 rate) | decays are audibly too slow; a period-1 envelope takes ~2 ticks per notch instead of 1 | `debug_envelope.*` |
| 2 | Sweep negative-mode bug: after applying a candidate, the SECOND calculation reads the stale pre-update shadow and its overflow-disable rule is missing entirely | negative sweeps descend at the wrong rate; a channel that should silence itself via second-update overflow keeps playing forever | `debug_sweep.negative_second_update_uses_fresh_shadow` |

`debug_sweep.positive_mode_is_unaffected` is an isolation guard: it must
pass even on the bugged skeleton. If it fails, you "fixed" the wrong
thing.

## Symptoms as audio hashes

The suite's committed probe drives a decaying, negatively-swept voice for
96 tick pairs and FNV-1a-64 hashes the raw `(freq & 0xFF, volume)` byte
stream:

```text
reference (solution): EF2E9FBF072A032B
bugged   (skeleton):  43D2649EDFE60D40
```

Run the drill suite to see the live divergence:

```bash
./ch17_90_debug_tests debug_probe      # skeleton: hash mismatch
# ... fix audio_debug.hpp ...
./ch17_90_debug_tests debug_probe      # matches EF2E9FBF072A032B
```

For a listenable equivalent, the committed probe *program*
`tests/public/ch17_gameboy_audio_accuracy/programs/probe_debug.apuprog`
runs through the exercise-04 runner; its golden PCM hash is recorded in
`goldens/goldens.md`. A student runner whose channels carry either bug
will not reproduce that hash.

## Method

1. Run `ch17_90_debug_tests`; pick ONE failing suite.
2. Hand-simulate that test's inputs against the committed model
   (SPEC.md of exercises 01).
3. Fix, re-run, then write `bug-report.md`:

```text
bug:
root cause:
first divergence:   (exact input where stub and truth part ways)
fix:
regression test:    (name of the TEST you would add to prevent a relapse)
```

Repeat until all tests pass. Do not fix both blind — the point is the
isolation workflow.
