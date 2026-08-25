# ch50 — PS1 Accuracy, Trace Testing and Compatibility

Turn "it boots" into "it is correct": golden CPU traces, VRAM and sample
hash corpora, per-subsystem state pins, ten seeded regressions, and an
aggregate accuracy-suite runner wired to the ctest label `accuracy`.

The chapter is self-contained: all models are the compact **psx-mini**
reference implementations defined in `shared/` (see `SPEC.md`). Nothing is
imported from other chapters, and no commercial ROMs are used — fixtures
are synthetic, committed as `.bin` + `.asm.txt` + provenance.

## Layout

| Dir              | Exercise                                                          |
|------------------|-------------------------------------------------------------------|
| `01_suite_runner` | aggregate runner `ch50_01_accuracy_runner` + suite-manifest units |
| `90_debug`        | ten seeded accuracy regressions (`seed01`..`seed10`)               |
| `91_challenge`    | full built-in suite green in-process + report hash pin            |

## Running the accuracy suites

All chapter tests carry the ctest label `accuracy`. From a build directory:

```bash
ctest -L accuracy            # run every accuracy-labelled test
```

(The repo-level `make accuracy` convenience target simply forwards to
`ctest -L accuracy`; adding the LABELS property to your tests IS the
integration contract — no shared-file edits needed.)

The aggregate runner reads a suite manifest — plain text, one
`case <name> builtin.<check>` or `case <name> <path> [args...]` line per
case — runs each check headless, prints PASS/FAIL per line plus a summary,
and exits 0 iff everything passed. From the repo root after building:

```bash
build/skels/ch50_ps1_accuracy_trace_testing/01_suite_runner/ch50_01_accuracy_runner \
  tests/public/ch50_ps1_accuracy_trace_testing/suites/suite.txt
```

That public suite exercises every built-in psx-mini check (CPU golden
trace, VRAM hash, SPU sample hash, DMA/GTE/timer/CDROM state pins) AND all
ten regression cases through their dedicated labstest filters, so it is a
complete one-command compatibility gate. The same file is what the hidden
manifest's aggregate case consumes.

## Gate checklist

- [ ] exercises: `ch50_01_runner_tests` GREEN; runner prints 7/7 PASS
- [ ] starter: `LABS=ch50_ps1_accuracy_trace_testing make skels && make build && make test`
- [ ] debug: all ten seeds diagnosed, `ch50_90_regress_tests seedNN` green for each, `bug-report.md` written
- [ ] challenge: `ch50_91_challenge_tests` GREEN (built-in suite passes in-process, report hash matches golden)
- [ ] coding_test: hidden manifest passes (`make grade GRADE_TARGETS=ch50_ps1_accuracy_trace_testing`)

## Verification

Verified with the isolated chapter harness:

```bash
VERIFY_PREFIX=/tmp/labs-ps1fin-ch50 tools/labs/verify_chapter.sh ch50_ps1_accuracy_trace_testing
# [verify] SKEL: build OK; ctest: 40% tests passed, 3 tests failed out of 5 (red failures expected here)
# [verify] SOLUTIONS: GREEN — 100% tests passed out of 5
# [verify] verdict: skel_build=ok solutions=GREEN
```

On the skeleton side exactly the expected cases fail RED: the ten seeded
regressions (each detected by its own `seedNN` test), the suite-parser
units (parse stub), and the challenge report-hash pin. All goldens were
generated twice from the reference solution and were byte-identical both
times (`cmp` of two runner runs). Every executable hidden case was then
executed directly against scratch-built solution binaries with the exact
manifest arguments — see `tests/hidden/ch50_ps1_accuracy_trace_testing/provenance.md`.
