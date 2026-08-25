# Challenge — ch25: Block Transfers

Curriculum goal: "Run ARM instruction tests." We do it with a synthetic
hand-assembled program plus the LDM/STM family implemented by *you*.

## Task

`challenge_ldm.hpp` contains two @LABS tasks:

1. `reg_count()` — register-list popcount.
2. `exec_block()` — full LDM/STM semantics:
   - IA (`P=0,U=1`) and DB (`P=1,U=0`) flavors,
   - optional writeback (`W`),
   - lowest register at lowest address,
   - no writeback when loading back into the base register,
   - cycle cost `(n+1)S + 1N` returned.

## Acceptance criteria

```bash
./ch25_challenge_runner \
    --rom ../../tests/public/ch25_arm7tdmi_arm_isa/roms/blockcopy.bin \
    --headless --cycles 500 --dump out.txt
diff out.txt ../../tests/public/ch25_arm7tdmi_arm_isa/goldens/blockcopy.dump
```

The fixture copies 8 words EWRAM-style using `STMIA`/`LDMIA`, checksums them
with a shift-add loop (barrel shifter + DP group), and parks in a self-loop.
All five debug-suite tests and the hidden block-transfer suite must pass.
