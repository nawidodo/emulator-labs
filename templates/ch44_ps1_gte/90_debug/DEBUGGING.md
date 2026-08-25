# Debugging exercise — ch44 GTE: saturation and divide-overflow bugs

Two seeded bugs live in `debug_gte.hpp` (`gtedbg::project`). The tests
here run RED on the untouched skeleton. Produce `bug-report.md` with
bug / root cause / first divergence / fix / regression test for each.

## Symptom guide

- `lm_clamps_negative_ir_to_zero` fails: the IR architectural write-back
  ignores the LM command bit. With LM=1 hardware clamps IR lanes to the
  UNSIGNED range 0..32767 and raises FLAG bit 27; a negative intermediate
  must never become a negative register value in that mode.

- `divide_overflow_raises_flag_and_forces_max` fails: when SZ3==0 the
  perspective divide overflows. Hardware raises FLAG bit 31, forces
  MAC0/IR0 to their documented maxima and clamps screen coordinates —
  it never silently substitutes a fake divisor or returns zeros.

## Acceptance

All three tests GREEN after your fixes + complete `bug-report.md`.
