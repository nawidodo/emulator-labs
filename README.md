# emulator-labs — Zero-to-Expert Game Emulator Engineering

A gated laboratory course that takes you from bit manipulation to a
PlayStation-class emulator. Modeled on Linux Kernel Labs: generated skeletons
with explicit `TODO(n)` checkpoints, reference solutions, committed unseen coding tests,
and hard chapter gates. Not a playlist of tutorials.

Derived from `docs/CURRICULUM-source.md`.

## Quickstart

```bash
make list                                   # see generatable chapters
LABS=ch01_lab_infrastructure make skels     # generate your skeleton
make test                                   # build + run (RED = TODOs waiting)
# ... implement TODO(1..N) in skels/ch01_lab_infrastructure/ ...
make test                                   # GREEN
python3 tools/labs/progress.py mark ch01_lab_infrastructure exercises passed
```

Full workflow, gates, hints: [docs/GATE.md](docs/GATE.md),
[docs/HINTS.md](docs/HINTS.md). Testing doctrine:
[docs/TESTING.md](docs/TESTING.md). Phase map:
[docs/ROADMAP.md](docs/ROADMAP.md).

## Choose your route

**Executable default (C++20):** CHIP-8 → 8080/Space Invaders → NES →
Playable NES gate → engineering (scheduler/save-states/debugger) → PS1.
Game Boy / GBA / SNES are optional depth branches and never block PS1.
Advanced NES mappers and dynarec are optional side-quests.

**Strict-C17 route (blueprint + seed):** `tracks/foundations-c17/` —
a separate executable track (CHIP-8 → 6502 → NES) with its own manifest;
today it carries the integer-model seed lab and the full curriculum
document as `docs/CURRICULUM-foundations-c17.md`.

Pick one answer to "what do I do first?":

```bash
python3 tools/labs/progress.py track ps1        # default PS1 route
python3 tools/labs/progress.py track classic-depth   # console depth
```

## The loop you will live in

```text
LECTURE -> SPEC -> EXERCISES -> STARTER -> DEBUGGING -> CHALLENGE
        -> CODE TEST -> REVIEW -> PASS -> next chapter unlocks
```

No theory compensates for broken code; the next chapter stays LOCKED until the
current gate passes all five components.

## Layout

```text
templates/   source of truth: lectures + annotated exercises (solutions inside)
skels/       YOUR working copies (generated)
solutions/   full reference trees (generated)
tests/       public fixtures + unseen grade manifests per chapter
tools/labs/  generate.py  skeleton generator (@LABS marker grammar)
             grade.py     unseen-test runner      progress.py gate tracker
             compare_trace.py / hash_frame.py    golden differ tools
             verify_chapter.sh isolated author verification
roms/        homebrew/test area — commercial ROMs NEVER enter this repo
docs/        AUTHORING contract, curriculum source, policies
third_party/ labstest.hpp minimal deterministic C++20 test framework
```

## Make targets

| Target | Meaning |
|---|---|
| `make list` | list generatable chapters/exercises |
| `LABS=t[,t...] [TODO=N] make skels` | generate skeleton(s), optionally resuming at TODO N |
| `make build` / `make test` | configure+build / ctest with failure output |
| `make debug` | Debug build |
| `make sanitize` | ASan+UBSan build + tests |
| `GRADE_TARGETS=chNN make grade` | unseen coding-test cases for a chapter |
| `make trace-test` | batch golden-trace comparison |
| `make solutions` | regenerate full solution trees |
| `make solution-build` / `solution-test` | build & run the reference trees |
| `make progress` | render the chapter gate table |

## Rules of the house

- Cores are headless-first and deterministic (`StepResult step()`, integer
  guest clocks, hashed frames/traces).
- Every CPU gets a disassembler early; every device is instantiable alone.
- Goldens must be deterministic AND independently validated against an
  accepted oracle/spec/test before becoming trusted fixtures
  (see docs/AUTHORING.md).
- Commercial ROM images are never committed; external suites attach via your
  own dumps (`roms/`, `requires_rom` gating).

## Status

Learner progress lives in gitignored `.emulator-labs/progress.json`;
course structure lives in `course-manifest.json`; author verification in
`verification.json`. View progress any time:

```bash
python3 tools/labs/progress.py status
```
