# ch11_gameboy_cpu_completion — SM83 Completion

Complete the Game Boy CPU: DAA, CB-page operations, stack/control flow,
HALT and interrupt handling.

## Layout

| Dir | Content | Status |
|---|---|---|
| `01_daa_rotates/` | DAA + rotates + CB page | done, verified |
| `02_stack_calls/` | PUSH/POP/CALL/RET/RST, conditional timing | done, verified |
| `03_halt_interrupts/` | HALT/EI-delay/IF/IE/dispatch | done, verified |
| `90_debug/` | four seeded CPU defects (`debug_cpu.hpp`) + DEBUGGING.md drill | done, verified |
| `91_challenge/` | smoke suite over two golden fixtures (`smoke_cpu`, `smoke_irq`) + `ch11_91_cpu_runner` | done, verified |
| `99_coding_test/` | unseen `$FF00`-page family (E0/F0/E2/F2/08) in `ldh.hpp` + `ch11_99_coding_test_runner` | done, verified |

Exercises include earlier headers relatively (`../01_daa_rotates/core.hpp`);
opcode families plug into the core via a hook chain (`Cpu::Hook`). The 91
machine lives in `91_challenge/machine.hpp` and is reused verbatim by the
99 runner (with the ldh hook added first in the chain).

## Gate checklist

- [x] exercises 01–03: skeletons RED -> solution GREEN
- [x] debug / challenge / coding test authored and verified
- [x] hidden manifest `tests/hidden/ch11_gameboy_cpu_completion/manifest.json`

## Verification

```
VERIFY_PREFIX=/tmp/labs-gbfin11 tools/labs/verify_chapter.sh ch11_gameboy_cpu_completion
# verdict: skel_build=ok solutions=GREEN   (exercises 01-03 + 90/91/99)
# skel RED as designed: ch11_90_debug fails on the four seeded bugs,
# ch11_91_smoke/ch11_99_coding_test fail on their stubs; everything GREEN
# in --mode solution.

# every non-optional hidden manifest case executed manually against the
# solution-tree binaries: daa_rot_contracts, stack_conditional_timing,
# irq_dispatch (labstest filters), smoke_golden + coding_test_ldh
# (runners: exit 0, expect_stdout_contains hit, fnv64 file hashes match).
```
