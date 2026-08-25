# Chapter 2 — C17 Integer Model for Emulation (seed lab)

Strict-C17 exercise: exact-width integers, endian-safe byte assembly,
sign extension, wrapping arithmetic, and defined-behavior shifts.

Build & run:

```bash
TRACK=foundations-c17 LABS=ch02_integer_model make skels
make build && ctest --test-dir build -R c17_ch002 --output-on-failure
```

Rules for this track: ISO C17 only (`-std=c17 -pedantic`), exact-width
types from `<stdint.h>`, no platform headers, deterministic pure
functions, TODO(n) markers mark unfinished work.
