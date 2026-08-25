# ch07_i8080_architecture

First real CPU: the Intel 8080. Registers, pairs, exact flag semantics
(S/Z/AC/P/CY), data-movement and ALU groups, instruction timing as a first
class part of `StepResult`.

## Layout

| Dir | Exercise | Artifact |
|---|---|---|
| `01_flags` | parity / S-Z-P / aux-carry primitives | `i8080_flags_tests` |
| `02_alu` | ADD/SUB/logical stage with exact CY+AC | `i8080_alu_tests` |
| `03_cpu_core` | CPU subset + headless runner + disassembler | `i8080_core_tests`, `i8080_runner` |
| `91_challenge` | straight-line arithmetic diagnostic program | `i8080_challenge_tests` |
| `99_coding_test` | ten unseen instructions (STAX/LDAX/INX/DCX/DAD/DAA/RLC/RRC/RAL/RAR) | `i8080_coding_tests` |

## Gate checklist

- [ ] exercises: every `NN_*` dir goes skel RED -> student GREEN
- [ ] starter: chapter generates, builds, runs (`make skels && make test`)
- [ ] debug: n/a this chapter (debugging exercise lands in ch08)
- [ ] challenge: 91 diagnostic passes with exact T-state count
- [ ] coding_test: hidden manifest (`tests/hidden/ch07_i8080_architecture/manifest.json`) fully passing

Runner CLI (canonical): `--rom PATH --cycles N --trace FILE --dump-state
FILE --headless --input-file FILE`. ROM loads at 0x0000; execution halts at
HLT or after N T-states.

## Verification

Recorded from authoring run (see provenance files under
`tests/public/ch07_i8080_architecture/`):

```bash
VERIFY_PREFIX=/tmp/labs-si8080 tools/labs/verify_chapter.sh ch07_i8080_architecture
# [verify] verdict: skel_build=ok solutions=GREEN
python3 tools/labs/generate.py --mode solution --targets ch07_i8080_architecture ...
# goldens regenerated twice, byte-identical:
#   tests/public/ch07_i8080_architecture/traces/ch07_diag.log
python3 tools/labs/grade.py --repo . ch07_i8080_architecture   # hidden cases pass
```
