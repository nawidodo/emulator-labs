# CHALLENGE — one core, three machines

Your interpreter must behave like a COSMAC VIP, a CHIP-48, or a modern
interpreter purely by switching a `Chip8Quirks` profile — no code edits,
no #ifdefs.

## Acceptance

```bash
./ch06_91_matrix
matrix: 15/15 PASS     # exit status 0
```

The harness runs five hand-assembled fixtures (`fixtures.hpp`, each also
committed as `.bin` + `.asm.txt` under `tests/public/ch06_chip8_accuracy_
debugger/fixtures/`) under all three profiles and compares an FNV-64 digest
of the final machine state against the golden table in `fixtures_golden.hpp`.

Every fixture isolates ONE quirk flag:

| fixture        | pins down              | who differs            |
|----------------|------------------------|------------------------|
| q1_shift       | shift_uses_vy          | VIP/CHIP48 vs MODERN   |
| q2_loadstore   | load_store_leaves_i    | VIP vs CHIP48/MODERN   |
| q3_jump        | jump_bnnn_x            | CHIP48 vs others       |
| q4_draw_wrap   | wrapping               | VIP vs others          |
| q5_vfreset     | vf_reset               | MODERN vs others       |

A matrix this shape cannot be passed by hardcoding: any single wrong flag
flips at least one digest pair.

## Hints

- The @LABS blocks mark every quirk-sensitive site in `chip8.hpp`.
- If exactly one profile fails everywhere, check `profile_by_name` first.
- Goldens were produced by the reference solution running twice with
  identical results; see `tests/public/ch06_chip8_accuracy_debugger/
  provenance.md` before suspecting the table.
