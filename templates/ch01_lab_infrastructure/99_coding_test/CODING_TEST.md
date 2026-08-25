# CODING TEST — ch01

The chapter gate ends here. Everything you solved so far is rehearsed;
this test asks you to apply your generator to something it has never
seen.

## The unseen template

A seven-checkpoint fixture template is mounted inside this exercise at
`data/seven_level/`:

- `core.cpp.tpl` — checkpoints **1, 3, 5, 7** (`//`-prefixed markers)
- `util.py.tpl` — checkpoints **2, 4, 6** (`#`-prefixed markers)
- `meta.ini` — no markers, must be copied verbatim

Its files spell markers with the `%` sentinel (`%LABS-BEGIN 1`, …), the
same convention as the `03` fixtures; your generator already accepts it.
Per-file sequences are independent — solving checkpoint 4 must not touch
checkpoint 5 in the other file.

## What "generate every valid skeleton version" means

Exactly nine variants exist for a seven-level template:

| variant      | invocation                       |
|--------------|----------------------------------|
| plain skel   | *(no --todo)*                    |
| resume 1..7  | `--todo N` for N = 1…7           |
| full solution| `--mode solution`                |

For every variant your generator must emit all three files plus a
deterministic `manifest.json`.

## How it is graded

The hidden manifest runs the checker binary built from this directory
against YOUR generated `generate_skel.py`:

```bash
build/skels/ch01_lab_infrastructure/99_coding_test/ch01_99_coding_test coding-test
build/skels/ch01_lab_infrastructure/99_coding_test/ch01_99_coding_test determinism
```

- `coding-test`: all nine variants; every output file (manifest included)
  must FNV-1a-64-match hashes recorded from the reference solution.
- `determinism`: one variant run twice must produce byte-identical trees.

The gate: **all output hashes match**. No partial credit — a single
misplaced line in any of the 36 generated files fails the variant.

## Hints

- Reuse, do not reimplement: your checkpoint 3 parser and checkpoint 4
  renderer already handle everything this template throws at you.
- If a variant mismatches, diff your output against the expected shape:
  stub bodies for unsolved blocks, solution bodies otherwise, text gaps
  untouched, marker lines gone.
- Watch the trailing-newline rule (`"\n".join(lines) + "\n"`).
