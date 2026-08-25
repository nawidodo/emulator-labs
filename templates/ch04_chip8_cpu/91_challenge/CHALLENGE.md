# Challenge — flags suite green under both quirk profiles

## Acceptance criteria

1. The committed course-original suite `tests/public/ch04_chip8_cpu/roms/
   flags_suite.bin` executes headless to a CLEAN halt (illegal_op == 0)
   under the COSMAC profile AND under the modern profile.
2. Final register state is IDENTICAL across the two profiles (V0-VF, PC,
   cycles). The suite is written quirk-neutral ON PURPOSE: self-shifts
   (X==Y makes the source moot), explicit `LD I` reloads around FX55/FX65,
   and a `LD VF,#01` renormalization after quirk-sensitive ops.
3. The quirk plumbing must be REAL: `quirks_probe.bin` MUST produce
   different final state under the two profiles.
4. `ch04_91_challenge_tests` encodes all of the above and passes.

Run it yourself:

```
<06 runner> --rom flags_suite.bin --headless --cycles 300     --trace cosmac.log
<06 runner> --rom flags_suite.bin --headless --cycles 300 --quirks modern     --trace modern.log
diff cosmac.log modern.log        # register fields identical, nothing else
```

## Stretch (optional, gated)

Timendus v5 CHIP-8 test suite covers opcode/flags/quirks behavior far beyond
one program: https://github.com/Timendus/chip8-test-suite — drop
`v5-opcode.bin` etc. under `roms/chip8/` and the hidden-manifest case
`timendus_v5_suite_optional` un-skips automatically. No network access is
required for the base challenge; the stretch is gated behind student-supplied
ROM per repo policy.
