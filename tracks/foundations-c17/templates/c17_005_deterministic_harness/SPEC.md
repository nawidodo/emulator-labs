# Chapter 5 — Deterministic Test Harness (seed lab)

Strict-C17 exercise: deterministic test harness primitives — suite runner,
scalar and memory expectations, FNV-1a 64-bit hashing, and hex dumping.
Every function is a deterministic pure function: no globals, no allocation,
no I/O beyond the caller's buffers except the harness's own diagnostic
prints.

Build & run:

```bash
TRACK=foundations-c17 LABS=c17_005_deterministic_harness make skels
make build && ctest --test-dir build -R c17_ch005 --output-on-failure
```

Rules for this track: ISO C17 only (`-std=c17 -pedantic`), exact-width
types from `<stdint.h>`, no platform headers, deterministic pure
functions, TODO(n) markers mark unfinished work.
