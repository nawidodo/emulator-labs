# ch40 — PS1 Interrupts and Timers

Build the two devices that make the PlayStation observable — the interrupt
controller (`I_STAT`/`I_MASK`) and the three root counters — plus the
deterministic event scheduler that drives them without a single wall-clock
read. See `LECTURE.md` for the full theory.

## Exercise map

| Exercise | What you build | Tests |
|---|---|---|
| `01_irq_controller` | `I_STAT` latch, write-1-clears ack, level reassert, `(I_STAT & I_MASK) != 0` output | `ch40_01_irq_controller` |
| `02_timers` | Root counters 0–2: clock sources (sysclk/dot/hblank/÷8), target/wrap events, one-shot/repeat/pulse/toggle IRQ, sync modes, readable flags | `ch40_02_timers` |
| `03_scheduler` | Deterministic event scheduler + bus + MIPS-I-subset CPU stub; integration test with trace lines `pc= op= irq= cyc=` | `ch40_03_scheduler`, runner CLI |
| `90_debug` | Seeded bug: acknowledge clears the WHOLE `I_STAT`, losing still-asserted sources. Fix it, write `bug-report.md` | `ch40_90_debug` |
| `91_challenge` | Run a synthetic fixture through the headless runner: Timer0 repeat mode, two interrupt periods served, golden trace + state hash | runner + goldens |
| `99_coding_test` | Unseen config image (`CTIM` format) → exact IRQ delivery order log; graded by hidden FNV-64 hash. Spec in `CODING_TEST.md` | hidden manifest |

## Gate checklist

- [ ] Exercises 01–03: skeleton builds and tests run RED
- [ ] Exercises 01–03: your implementation reaches GREEN
- [ ] 90_debug: bug found, fixed, `bug-report.md` written
- [ ] 91_challenge: acceptance criteria in `CHALLENGE.md` met against the committed goldens
- [ ] 99_coding_test: hidden order-hash passes

## Fixture policy

All programs are synthetic MIPS-I images assembled with a throwaway
assembler; sources of truth are the `.asm.txt` listings under
`tests/public/ch40_ps1_interrupts_timers/roms/` with provenance notes.
Commercial ROMs never enter the repo.

## Verification

Recorded from authoring (repo root):

```bash
VERIFY_PREFIX=/tmp/labs-ch40irq tools/labs/verify_chapter.sh ch40_ps1_interrupts_timers
#   [verify] verdict: skel_build=ok solutions=GREEN

python3 tools/labs/grade.py --repo . ch40_ps1_interrupts_timers   # after make build
```

Hidden-manifest cases were additionally executed directly against the
solution-tree binaries with hashes recomputed; see
`tests/public/ch40_ps1_interrupts_timers/provenance.md` for the exact
commands and hash values.
