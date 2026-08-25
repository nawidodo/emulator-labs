# Challenge — full dynarec pipeline, bit-exact end to end (ch37)

Run the rx8 benchmark through the COMPLETE pipeline — basic-block analysis,
IR translation, optimization — and prove it is **bit-exact** against the
plain switch interpreter on every workload, including self-modifying code.

## Why this is the chapter in one exercise

Each stage was individually tested in exercises 01–04. Real emulators fail
at the seams: an optimizer that ignores SMC invalidation, a translation
cache that outlives the bytes it decoded, a fused op that changes fault
behavior. The acceptance bar is deliberately brutal:

- identical observable dumps (OUT log + memory) across switch interpreter,
  unoptimized IR, and optimized IR;
- the optimized pipeline executes at least 20% fewer IR ops than the
  unoptimized one on the benchmark-shaped workload (`pipeline.threshold`);
- SMC workloads still flush stale translations after optimization.

A native host JIT stays optional — the portable IR pipeline already IS a
dynamic recompiler in structure: decode once, optimize once, execute many.

## Where to work

`main.cpp` contains annotated stubs (`@LABS` blocks 1–2): the offline
translate step and the pipeline driver that installs (optionally optimized)
blocks into an `IrEngine`. All five supporting headers are provided
already-solved; your job is purely correct orchestration.

## Acceptance

Every `challenge.*` TEST passes:

```bash
ctest --test-dir build -R ch37_91_challenge --output-on-failure
```
