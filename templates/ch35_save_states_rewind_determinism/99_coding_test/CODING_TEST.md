# Coding test — load-state-100-times determinism (ch35)

A state loaded 100 times must produce the exact same later framebuffer
hash, every time. That is the contract deterministic replay depends on.

## Protocol (implemented in `determinism100.cpp`)

1. Boot `drift.bin`-style synthetic ROM (the hidden fixture uses a
   different program), run 30 frames.
2. Save a state blob.
3. 100 times: fresh machine → load that exact blob → run 60 frames →
   hash the raw 64x32 framebuffer.
4. All 100 hashes must equal the first. Write to `--out FILE`:

```text
determinism=ok hash=<fnv64-hex>
```

and exit 0 — or list mismatches (`mismatch i=7 got=... want=...`) and
exit 1.

## What this catches

Uninitialized padding in states, wall-clock or unseeded-RNG leakage,
frontend resources smuggled into "machine" state — any of these flips at
least one of the 100 hashes. The seeded non-determinism demo from the
lecture (serialize padding vs not) is exactly one such failure.

## Grading

The hidden manifest runs your binary and hashes the result file; the
`hash=` value must match the reference solution byte-for-byte.
