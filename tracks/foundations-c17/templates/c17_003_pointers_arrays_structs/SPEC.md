# Chapter 3 — C17 Pointers, Arrays, Structs, and Ownership (seed lab)

Strict-C17 exercise: byte-wise little-endian de/serialization, pointer
arithmetic, arrays, struct fields, and byte-array ownership. Every
function is a deterministic pure function: no globals, no allocation,
no I/O beyond the caller's buffers.

Build & run:

```bash
TRACK=foundations-c17 LABS=c17_003_pointers_arrays_structs make skels
make build && ctest --test-dir build -R c17_ch003 --output-on-failure
```

Rules for this track: ISO C17 only (`-std=c17 -pedantic`), exact-width
types from `<stdint.h>`, no platform headers, deterministic pure
functions, TODO(n) markers mark unfinished work.