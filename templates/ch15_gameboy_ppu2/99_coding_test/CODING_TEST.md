# Coding test — repair from a snapshot

The hidden grader (`make grade GRADE_TARGETS=ch15_gameboy_ppu2`) runs the
chapter runner and the debug suites against fixtures you have never seen.
There is nothing to implement here — this file describes the methodology
the test rewards.

## Setup

Finish `90_debug` (all five defects fixed, bug-report.md written) and
exercises 01-04. Then:

```bash
make grade GRADE_TARGETS=ch15_gameboy_ppu2
```

The hidden cases render unseen snapshots and hash both the RGBA frame and
the mode-transition trace (`--trace`), plus rerun the `debug_lyc` suite.

## Methodology: trace-first divergence isolation

1. **Reproduce with the trace, not the image.** Run your runner on the
   failing scenario with `--trace /tmp/t.log`. Timing bugs (a mode
   transition off by a line, an LYC compared early) show up as a
   diffable text divergence long before they are visible in pixels.
2. **Find the FIRST diverging line/entry.** For frames: dump single
   scanlines from a scratch test and binary-search for the first row that
   differs. For traces: diff against the committed public golden under
   `tests/public/ch15_gameboy_ppu2/traces/`.
3. **Map divergence to hardware cause.** A wrong dot in the trace → mode
   threshold arithmetic; a wrong pixel only where two sprites overlap →
   priority ordering; wrong content AFTER a disabled stretch → window
   internal counter; everything shifted one sprite → OAM scan order or
   the x==0 rule.
4. **Fix the model, not the symptom.** Every defect in this chapter is a
   one-line wrong invariant; re-derive it from Pan Docs rather than
   special-casing the fixture.
5. **Re-grade.** All non-optional hidden cases must pass; the optional
   mooneye case skips honestly unless you supply the ROM yourself.
