# ch31_snes_65c816_cpu — 65C816 CPU

Scaffold for the SNES CPU chapter: width switching, bank registers and
a small but faithful executor with trace + disassembler.

## Layout note

Exercises are generated as ONE tree per chapter (the challenge
includes `../03_execute/exec.hpp`), so always generate the chapter
target, not a single exercise:

```sh
LABS=ch31_snes_65c816_cpu make skels      # student skeleton
make solutions                            # reference solution
```

## Gate checklist

| Component | Artifact |
|---|---|
| exercises | 01_widths, 02_addressing, 03_execute: skel RED -> student GREEN |
| starter   | chapter builds; runner CLI works (`ch31_03_exec_runner --help`) |
| debug     | 90_debug seeded bug + DEBUGGING.md + bug-report.md |
| challenge | 91_challenge golden trace + memory contract |
| coding_test | tests/hidden/ch31_snes_65c816_cpu/manifest.json passes |

## Runner

```sh
ch31_03_exec_runner --rom roms/demo.bin --cycles 1000 --trace t.log
```

- ROM images map sequentially into banks from $00:$0000.
- Reset state: emulation mode, PC=$0000, SP=$01FF, M=X=I flags set.
- BRK (opcode $00) halts deterministically.
- Trace lines: `pc=<hex> op=<hex> k=.. db=.. a=.. x=.. y=.. p=..
  sp=.. cyc=<n>` (lowercase keys, `cyc` last).
- `--headless`, `--frames`, `--hash-frame`, `--input-file` are
  accepted for CLI parity and are no-ops in a CPU-only system.

## Verification

See the "Verification" section at the bottom of this file for the
exact commands run when this chapter was authored.

## References

See LECTURE.md.
## Verification

```text
VERIFY_PREFIX=/tmp/labs-SNES-ch31 tools/labs/verify_chapter.sh ch31_snes_65c816_cpu
[verify] SKEL: build OK; ctest: 14% tests passed, 6 tests failed out of 7
         (red failures expected here)
[verify] SOLUTIONS: GREEN — 100% tests passed out of 7
[verify] verdict: skel_build=ok solutions=GREEN
```

Hidden cases validated by executing the scratch solution binaries with
the exact manifest args:

- `widths.xce_enter_emu_clears_index_high` -> exit 0
- runner trace golden -> FNV64 E4DEE178096106C9 (matches manifest)
- `coding.` filter -> exit 0
