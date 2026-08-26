# Chapter 4 — Build System, Assertions, and Sanitizers (seed lab)

Strict-C17 exercise: build-system hygiene, checked arithmetic, power-of-two
alignment, decimal byte parsing, assertions, and sanitizer-visible memory
handling. Every function is a deterministic pure function: no globals, no
allocation, no I/O beyond the caller's buffers.

Build & run:

```bash
TRACK=foundations-c17 LABS=c17_004_build_sanitizers make skels
make build && ctest --test-dir build -R c17_ch004 --output-on-failure
```

Rules for this track: ISO C17 only (`-std=c17 -pedantic`), exact-width
types from `<stdint.h>`, no platform headers, deterministic pure
functions, TODO(n) markers mark unfinished work.
