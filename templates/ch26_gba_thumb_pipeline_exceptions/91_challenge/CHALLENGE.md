# Challenge — ch26: Stack Operations

Curriculum goal: model the Thumb call/return discipline. PUSH and POP are
the only stack instructions a GBA game needs for nested calls — compilers
emit them at every function boundary.

## Task

`challenge_cpu.hpp` contains two @LABS tasks on top of the 02_pipeline
core:

1. `exec_push(list)` — full-descending PUSH:
   - start address `SP - 4*count`, lowest register at the lowest address,
   - bit8 of the list is LR, stored at the highest address,
   - SP writeback to `SP - 4*count`,
   - modeled cost `n + 1` cycles.
2. `exec_pop(list)` — POP with post-increment:
   - ascending addresses into ascending registers,
   - bit8 of the list is PC: load it, mask bit0 (ARMv4T keeps the T state),
     pay a branch refill,
   - SP advances past every transferred word.

## Acceptance criteria

```bash
./ch26_challenge_runner \
    --rom ../../tests/public/ch26_gba_thumb_pipeline_exceptions/roms/callret.bin \
    --headless --cycles 40 --dump out.txt
diff out.txt ../../tests/public/ch26_gba_thumb_pipeline_exceptions/goldens/callret.dump
```

The fixture calls a clobbering helper through `PUSH {LR}` / `POP {PC}`,
restores caller state with `PUSH {r0,r1}` / `POP {r0,r1}`, sums the restored
values and parks. All five challenge tests plus the hidden stack suite must
pass.
