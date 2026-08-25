# ch08_i8080_control_interrupts

The 8080 control-flow subsystem: CALL/RET/RST, conditional branches with
exact taken/not-taken timing, PUSH/POP (incl. PSW), EI/DI and the interrupt
acknowledge model. Debugging exercise seeds a timing bug and a stack bug;
the coding test is the first-divergence harness.

## Layout

| Dir | Exercise | Artifact |
|---|---|---|
| `01_stack` | push/pop mechanics + PSW packing | `i8080_stack_tests` |
| `02_control_flow` | jumps/calls/returns/RST/interrupts + runner | `i8080_flow_tests`, `i8080_flow_runner` |
| `90_debug` | seeded bugs: swapped call cycles + swapped pop bytes | `i8080_debug_tests` |
| `91_challenge` | course-original diagnostic ROM w/ golden trace+dump | `i8080_diag_tests`, `i8080_diag_runner` |
| `99_coding_test` | first-divergence identification harness | `i8080_divergence_tests` |

## Gate checklist

- [ ] exercises: skel RED -> student GREEN
- [ ] starter: chapter generates, builds, runs
- [ ] debug: fix seeded bugs + produce bug-report.md
- [ ] challenge: diagnostic passes with exact cycle count; trace matches golden
- [ ] coding_test: hidden manifest fully passing

## Verification

```bash
VERIFY_PREFIX=/tmp/labs-si8080 tools/labs/verify_chapter.sh ch08_i8080_control_interrupts
# [verify] verdict: skel_build=ok solutions=GREEN
python3 tools/labs/grade.py --repo . ch08_i8080_control_interrupts
```

Golden trace/dump provenance: `tests/public/ch08_i8080_control_interrupts/provenance.md`.
