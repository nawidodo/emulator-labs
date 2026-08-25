# Debugging exercise — ch46: MDEC coefficient-order and clamp bugs

Two seeded bugs live in `debug_mdec.hpp`. Tests run RED until fixed.
Produce `bug-report.md` with bug / root cause / first divergence / fix /
regression test for each.

## Symptom guide

- `coefficients_land_in_natural_order` fails with values scattered to the
  wrong indices: the decoder already emits NATURAL order (it applies the
  zig-zag table internally). Applying a second un-scan pass scatters
  coefficients again — the classic "diagonal garbage" MDEC bug.

- `samples_saturate_not_wrap` fails on out-of-range samples: pixel data
  must SATURATE at 0 and 255. Masking to 8 bits wraps bright ringing into
  black stripes.

## Acceptance

All tests GREEN after fixes + complete `bug-report.md`.
