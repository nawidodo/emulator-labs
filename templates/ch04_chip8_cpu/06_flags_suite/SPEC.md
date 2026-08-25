# SPEC — flags_suite.bin (course-original test program)

Hand-assembled opcode/flags coverage program (see `roms/flags_suite.bin` +
`.asm.txt`; no commercial ROM content). Sections:

1. ALU flag rules: ADD carry out, SUBN/SUB borrow conventions.
2. Quirk-neutral shifts: self-shift forms (8XY6/8XYE with X==Y) whose result
   does not depend on `shift_uses_vy`.
3. Memory: FX33 BCD digit order, FX29 font lookup, FX55/FX65 roundtrip with
   explicit `LD I` reloads (neutralizes load_store_leaves_i) and a
   `LD VF,#01` renormalization (neutralizes vf_reset).
4. Control flow: countdown loop with SE exit.
5. Deterministic RNG: CXNN against the machine's fixed default seed.
6. Keypad: SKNP skip over a trap to the success marker (`LD VE,#42`).

Acceptance: clean halt (illegal_op == 0) under BOTH quirk profiles with
identical V0-VF / PC / cycles / I state. Goldens: `../traces/
flags_suite{,_modern}.golden.log`, `../regs/flags_suite{,_modern}.json`;
generation commands in `../provenance.md`.

`quirks_probe.bin` is the negative control: it MUST diverge across profiles
(shift source + I advance), proving the quirk switches are wired.
