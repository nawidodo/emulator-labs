# Chapter 13 — Game Boy Timers and Interrupts

SM83 timer hardware (DIV/TIMA/TMA/TAC), the IF/IE interrupt lines, and
full interrupt dispatch with priority arbitration — built exactly and
deterministically on top of a self-contained copy of the chapter-11 CPU
interface.

The CPU interface (`bus.hpp`, `cpu.hpp`, `opcode_meta.hpp`,
`int_ctl.hpp`) lives in `01_divider/` as a trimmed copy of ch11
(fetch/step/hooks + ALU/loads/jumps + the IntCtl/IrqHook/IntBus interrupt
machinery). Later exercises include it via `../01_divider/*.hpp`; nothing
is included across chapters.

## Layout

| directory | subject | target |
|-----------|---------|--------|
| `01_divider/` | free-running counter, DIV read/write semantics (+ CPU copies) | `ch13_01_divider_tests` |
| `02_tima_edge/` | TAC gate + exact bit-select table, falling-edge TIMA ticks | `ch13_02_edge_tests` |
| `03_overflow_reload/` | immediate TMA reload + IF bit 2 raise policy | `ch13_03_reload_tests` |
| `04_interrupt_delivery/` | dispatch to vectors, priority, HALT wake rules | `ch13_04_irq_tests` |
| `90_debug/` | three seeded timer defects + bug-report.md drill | `ch13_90_debug_tests` |
| `91_challenge/` | assembled timer probe program + headless runner with interrupt-log goldens | `ch13_91_timer_tests`, `ch13_91_timer_runner` |
| `99_coding_test/` | the three debug defects restated as behavioral contracts | `ch13_99_coding_test_tests` |

Fixtures and goldens live under `tests/public/ch13_gameboy_timers_interrupts/`
(program listings + binaries under `fixtures/`, golden log + state under
`goldens/`); hidden grading material under
`tests/hidden/ch13_gameboy_timers_interrupts/`.

## Model contract highlights

* One internal 16-bit divider counter gaining one count per T-cycle;
  DIV = high byte (steps every 256 T-cycles); any write resets it all.
* Exact bit-select table (TAC bits -> tapped bit): `00->9, 01->3, 10->5,
  11->7` — tick periods 1024/16/64/256 T-cycles.
* TIMA increments on gated falling edges of the tapped bit, sampled once
  per 4-T-cycle block (sampling never stops).
* Disabling TAC while the tapped bit reads 0 after the write produces
  exactly one increment (the disable edge).
* Overflow reloads TMA immediately and raises IF bit 2 (real hardware
  delays both by 4 T-cycles — documented simplification, see LECTURE.md).
* Dispatch priority VBlank > STAT > Timer > Serial > Joypad; 20 cycles;
  EI-delay and RETI semantics identical to ch11.

## Gate checklist

- [ ] exercises 01-04: skeleton RED -> student GREEN
- [ ] starter builds and runs (`make skels && make test`)
- [ ] debug: all three seeded defects fixed + `bug-report.md`
- [ ] challenge: probe program passes through the runner with the golden log
- [ ] coding test: hidden manifest fully passing (unseen .bin variants hash
      their interrupt logs)

## Verification

From the repo root:

```bash
VERIFY_PREFIX=/tmp/labs-gbfin13 tools/labs/verify_chapter.sh \
    ch13_gameboy_timers_interrupts
```

verdict: `skel_build=ok solutions=GREEN` (the skeleton builds with its
tests RED — the only passing skeleton case is the runner `--help` smoke
test; the solutions tree passes all 8 ctest cases, warning-free under
`-Wall -Wextra -Wpedantic`). Every non-optional hidden manifest case was
additionally executed by hand against the solution-tree binaries and
passed; details are recorded in
`tests/hidden/ch13_gameboy_timers_interrupts/provenance.md`.
