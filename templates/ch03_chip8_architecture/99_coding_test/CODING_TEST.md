# CODING TEST — five unseen instructions

Implement from this specification alone. No reference implementations, no
test-driven hints: the visible suite only checks that the machine still
boots. The hidden grader executes crafted ROM images through the headless
runner and compares full traces, so the semantics must be exact.

## Where to work

`chip8.hpp` contains five `op_*` functions marked with `TODO(n)` blocks.
Fill them in; do not change any other file. `runner_main.cpp` is already a
complete headless runner (`--rom --headless --cycles N --frames N --trace F
--hash-frame F`) for your own experiments.

## Machine conventions (already implemented)

- PC advances past an instruction *before* it executes; trace `pc=` shows
  post-execution PC.
- Stack: 16 words. Push = `stack[SP++] = pc`; pop = `pc = stack[--SP]`.
- SP counts entries: 0 when empty.

## Instructions to implement

### 1. `2NNN` — CALL addr (@LABS block 1, `op_call`)

Push the address of the next instruction (the already-advanced PC) onto the
stack, then set `PC = NNN`.

### 2. `00EE` — RET (@LABS block 2, `op_ret`)

Pop the top of the stack into `PC`.

### 3. `3XNN` — SE Vx, byte (@LABS block 3, `op_se_vx_nn`)

Skip the next instruction if `VX == NN`. "Skip" means PC advances by one
extra instruction word (2 bytes).

### 4. `4XNN` — SNE Vx, byte (@LABS block 4, `op_sne_vx_nn`)

Skip the next instruction if `VX != NN`.

### 5. `BNNN` — JP V0 + addr (@LABS block 5, `op_jp_v0`)

Set `PC = NNN + V0`. Note the sum may exceed 12 bits; PC is 16-bit and no
masking is specified — keep the full 16-bit sum.

## Acceptance

The hidden manifest runs ROM images like:

```text
0200: 22 0A      CALL subroutine at 0x20A
0202: ...
```

Your implementation must reproduce exact register/PC traces for CALL/RET
round-trips, taken and not-taken skips for both SE/SNE, and computed jumps
with a nonzero V0. When your visible suite passes and you can hand-execute
all three examples above correctly, submit:

```bash
python3 tools/labs/grade.py --repo . ch03_chip8_architecture
```
