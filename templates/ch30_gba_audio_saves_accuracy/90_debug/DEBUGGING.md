# DEBUGGING — ch30: save-game corruption

Two seeded defects live in `save_bugs.hpp`. Player-visible symptoms:

1. **Saves "un-corrupt" themselves after reload.** Programming writes the
   raw byte instead of AND-ing into the cell, so bits erased to 0 get set
   back to 1 in memory that was never re-erased. Checksums written before a
   second program pass then mismatch.
2. **64 KiB saves wrap into garbage.** Bank/address masking ignores the
   device size, so addresses above 0xFFFF (or banked writes on small chips)
   alias into unrelated offsets.

## Your task

1. Find both defects (`main.cpp` encodes real behavior).
2. Fix them.
3. Write `bug-report.md`: bug / root cause / first divergence / fix /
   regression test.
