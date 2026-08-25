# The Chapter Gate

Every chapter runs the same loop (curriculum §4):

```
LECTURE            read LECTURE.md in templates/chNN_slug/
   ↓
SOURCE/SPEC        study SPEC.md files + linked primary references
   ↓
EXERCISES          LABS=chNN_slug make skels ; implement TODO(1..N)
   ↓
STARTER            chapter builds & runs end-to-end (make test)
   ↓
DEBUGGING          90_debug: seeded bugs; file bug-report.md
   ↓
CHALLENGE          91_challenge acceptance criteria
   ↓
CODE TEST          make grade GRADE_TARGETS=chNN_slug
   ↓
REVIEW             reread diffs vs solution; run solution-tree diff if desired
   ↓
PASS               mark every component the chapter defines;
                   next dependency unlocks
```

Marking a gate:

```bash
python3 tools/labs/progress.py mark ch01_lab_infrastructure exercises passed
python3 tools/labs/progress.py mark ch01_lab_infrastructure starter    passed
python3 tools/labs/progress.py mark ch01_lab_infrastructure debug      passed
python3 tools/labs/progress.py mark ch01_lab_infrastructure challenge  passed
python3 tools/labs/progress.py mark ch01_lab_infrastructure coding_test passed
# -> "GATE PASSED: ch01 -> ch02 ACTIVE"
```

A chapter unlocks when **all of its explicit prerequisites** pass —
`course-manifest.json` `requires[]` is the source of truth, not chapter
numbers. Chapter number does not imply dependency; optional tracks (Game
Boy / GBA / SNES on the PS1 path) never block unrelated tracks.

## Component semantics

| Component | Pass condition |
|---|---|
| exercises | every `NN_*` exercise suite green under `make test` |
| starter | skeleton generated, builds, runner CLI works (`--help`, smoke ROM) |
| debug | `90_debug` fixed AND bug-report.md written (bug/root cause/first divergence/fix/regression test) |
| challenge | `91_challenge` acceptance criteria met (hashes/traces match) |
| coding_test | `make grade GRADE_TARGETS=chNN_slug` exits 0 |

## Track selection

```bash
python3 tools/labs/progress.py track ps1     # or classic-depth / nes-depth / core
```

With an active track, next-chapter suggestions defer optional chapters that
do not belong to it. Infrastructure invariants are machine-checked:

```bash
make audit      # manifest/DAG auditor: cycles, roots, optional leakage
```

## Resuming mid-exercise

Skeletons regenerate idempotently; completed checkpoints stay solved:

```bash
TODO=4 LABS=ch03_chip8_architecture/02_fetch_decode make skels
```
