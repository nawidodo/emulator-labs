# Debugging — ch27 GBA memory system

The `90_debug` bus builds and runs, but reports wrong values and cycle
counts. Five defects are seeded:

1. **Post-branch accesses are too cheap.** After a pipeline refill the
   first access must bill non-sequential; the refill signal is currently
   ignored whenever addresses happen to be adjacent.
   (test: `bug1_refill_forces_nonsequential_cost`)

2. **Only the first 64 K of EWRAM answers.** Offsets above 64 K alias back
   into the first block instead of using the full 256 K window.
   (test: `bug2_ewram_mirrors_every_256k`)

3. **Sprite tiles appear over background data.** VRAM routing folds with a
   plain 16-bit mask, aliasing the OBJ bank onto the BG bank.
   (test: `bug3_vram_obj_bank_is_not_bg`)

4. **SRAM leaks neighbouring bytes.** The 8-bit chip answers with two
   bytes as if it were halfword-wide.
   (test: `bug4_sram_reads_answer_one_byte`)

5. **Reads of unmapped space return zero** instead of the last value
   driven onto the data bus.
   (test: `bug5_open_bus_latches_last_value`)

## Deliverable

Fix all five in `debug_bus.hpp`, then write `bug-report.md` with, per bug:
bug / root cause / first divergence / fix / regression test.
