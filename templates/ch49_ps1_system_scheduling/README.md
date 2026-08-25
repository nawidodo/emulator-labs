# ch49 — Complete PS1 System Scheduling

Integrate the PS1-phase components behind a COMMON EVENT SCHEDULER: a
master clock at 33.8688 MHz, a GPU busy model on the 53.2224 MHz dot
clock (exactly 11/7 x CPU), DMA bus stalls, CD latency events, SPU
sample-period events at exactly 44100 Hz (= 768 CPU cycles per sample),
and INTC latch ordering that is deterministic by construction.

The full subsystems live in their own chapters and cross-chapter imports
are forbidden, so this chapter defines minimal stand-in devices (tiny
synthetic RISC core, GPU/DMA/CD/SPU/INTC timing models). The composition
is documented in LECTURE.md + `02_mini_devices/SPEC.md`.

## Layout

| Dir              | Exercise                                                       |
|------------------|----------------------------------------------------------------|
| `01_scheduler_core` | event queue: schedule/cancel/dispatch-to-deadline, FIFO ties |
| `02_mini_devices`   | stand-in GPU/DMA/CD/SPU/INTC + mini core, all event-driven   |
| `03_boot_runner`    | headless runner: ROM boot, canonical traces, hashed logs     |
| `90_debug`          | seeded wrong-order tie-break (SPU IRQ jumps the queue)       |
| `91_challenge`      | boot a real assembled cd -> spu -> gpu handshake program     |
| `99_coding_test`    | hidden unseen-program event-log hash contract                |

## Gate checklist

- [ ] exercises: all `ch49_*_tests` RED on skeleton, GREEN when solved
- [ ] starter: `LABS=ch49_ps1_system_scheduling make skels && make build && make test`
- [ ] debug: fix the seeded tie-break defect, write `bug-report.md`
- [ ] challenge: `ch49_91_challenge_tests` GREEN against the public golden
- [ ] coding_test: hidden manifest passes (`make grade GRADE_TARGETS=ch49_ps1_system_scheduling`)

## Verification

Verified with the isolated chapter harness:

```bash
VERIFY_PREFIX=/tmp/labs-ps1fin-ch49 tools/labs/verify_chapter.sh ch49_ps1_system_scheduling
# [verify] SKEL: build OK; red failures expected here
# [verify] SOLUTIONS: GREEN
# [verify] verdict: skel_build=ok solutions=GREEN
```

Every executable hidden case was additionally executed directly against
scratch-built solution binaries with the exact manifest arguments; see
`tests/hidden/ch49_ps1_system_scheduling/provenance.md`. Golden hashes
were generated twice from the reference solution and are byte-identical.
