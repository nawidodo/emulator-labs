# Chapter 6 — CHIP-8 Accuracy, Quirks and the Debugger

Phase II closes here. Your CHIP-8 runs games; now we make it *trustworthy*.
This chapter teaches the discipline that every later system (8080, Game
Boy, NES, PS1) depends on: when something looks wrong on screen, you do not
stare at pixels — you diff traces.

## Why emulators disagree

CHIP-8 was implemented at least four times with slightly different opcode
semantics, and most software was written against the 1977 COSMAC VIP:

| behavior                | COSMAC VIP        | CHIP-48 (1990s fork) | MODERN            |
|-------------------------|-------------------|----------------------|-------------------|
| `8XY6`/`8XYE` shift     | shifts **VY**→VX  | shifts VY→VX         | shifts VX in place|
| `FX55`/`FX65` leave I   | yes               | no (`I += X+1`)      | no                |
| `BXNN` jump base        | V0                | **VX** (the bug)     | V0                |
| `DXYN` off-screen pixels| wrap around       | clip                 | clip              |
| `FX1E`/`55`/`65` vs VF  | VF untouched      | VF untouched         | VF cleared first  |

A ROM assembled for one variant silently misbehaves elsewhere. The fix is
not "pick the right one" — it is making the choice an explicit,
switchable configuration: a quirk profile.

## The debugger toolkit

Curriculum §54–§56 make three things mandatory for every future CPU, and
you build all of them here over the same small core:

- **Instruction traces** — one line per executed instruction
  (`pc=0200 op=00E0 V0=00 I=000 SP=0F cyc=11`), canonical mode for quick
  diffs, full mode (`--trace-full`) dumping every register plus both timers.
- **Stepping** — execute exactly N instructions, observe state between them.
- **Breakpoints / watchpoints** — stop before an address executes; stop
  when memory changes or a register predicate (`V3==2A`) becomes true.
- **Register dumps, memory dumps, disassembly** — the `regs`, `memory`,
  `disasm` commands of a scriptable REPL.
- **Golden traces & screenshots** — reference outputs produced by a known
  good implementation; your output must match byte-for-byte. Trace-first
  debugging means finding the FIRST divergence, never the last symptom.

## Exercises

| exercise           | what you build                                            |
|--------------------|-----------------------------------------------------------|
| `01_tracing`       | canonical + full trace writers, golden-trace workflow      |
| `02_debugger_repl` | scripted REPL: step/continue/regs/memory/break/disasm      |
| `03_watchpoints`   | memory-range watchpoints + register predicates             |
| `04_quirk_profiles`| `struct Chip8Quirks` + COSMAC_VIP/CHIP48/MODERN presets    |
| `05_compat_hunt`   | locate a real wrong-profile divergence with YOUR debugger  |
| `90_debug`         | seeded FX55 bug: trace it, fix it, write the bug report    |
| `91_challenge`     | profile matrix harness — 15/15 golden digests or bust      |
| `99_coding_test`   | mystery ROM + wrong-profile excerpt; find the true profile |

Every runner speaks the standard headless CLI (`--rom --cycles/--frames
--trace [--trace-full] --hash-frame --input-file --quirks PROFILE`) and the
debuggers accept `--script FILE` for byte-deterministic sessions.

## Gate checklist

- [ ] exercises: `ch06_01_trace` … `ch06_05_hunt` red in skeleton, green in solution
- [ ] starter: chapter generates, builds, tests via `make skels && make test`
- [ ] debug: `90_debug` fixed + `bug-report.md` written by you
- [ ] challenge: `ch06_91_matrix` prints `matrix: 15/15 PASS`
- [ ] coding_test: hidden manifest fully passing (`make grade GRADE_TARGETS=ch06_chip8_accuracy_debugger`)

Optional hardware suites (Timendus CHIP-8 test suite,
https://github.com/Timendus/chip8-test-suite) are referenced but gated:
drop the ROM under `roms/chip8/timendus/` yourself if you want to run them;
grading skips gracefully when absent.

## Progress tracking

Record your gate results with `tools/labs/progress.py`:

```bash
python3 tools/labs/progress.py status
python3 tools/labs/progress.py mark ch06_chip8_accuracy_debugger exercises passed
python3 tools/labs/progress.py mark ch06_chip8_accuracy_debugger starter passed
python3 tools/labs/progress.py mark ch06_chip8_accuracy_debugger debug passed
python3 tools/labs/progress.py mark ch06_chip8_accuracy_debugger challenge passed
python3 tools/labs/progress.py mark ch06_chip8_accuracy_debugger coding_test passed
python3 tools/labs/progress.py unlock-check ch07_8080_cpu_core
```

The status table follows the curriculum §60 format (Exercises / Starter /
Debug / Challenge / Code Test columns); chapter 7 unlocks only when ALL
five components of this chapter are marked passed.

## Verification

What was run when this chapter shipped (from repo root):

```bash
VERIFY_PREFIX=/tmp/labs-ch06 tools/labs/verify_chapter.sh ch06_chip8_accuracy_debugger
# verdict: skel_build=ok solutions=GREEN
```

Skeleton tree builds with its tests RED; solution tree builds and passes
every ctest. The hidden manifest cases were additionally executed against
the built solution binaries with their exact manifest arguments.

## Graduation milestone

**You have built your first emulator.**

Fetch → decode → execute, registers, memory, stack, timers, input,
display, quirks, traces, breakpoints, watchpoints, golden tests. That is
the complete loop that scales from 4 KB CHIP-8 to a PlayStation. Phase III
starts dragging the training wheels: a real CPU with real instruction
timing, where everything you learned here becomes daily practice.
