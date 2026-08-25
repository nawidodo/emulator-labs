# ch38 — MIPS R3000A CPU

Interpreter for the PlayStation's MIPS I CPU core with correct branch-delay
slot semantics. See `LECTURE.md` for the full hardware lecture.

## Exercise map

| Dir | Content | Gate |
|-----|---------|------|
| `01_alu` | ALU/shift instruction groups | exercises |
| `02_mem_ops` | loads/stores + lwl/lwr/swl/swr unaligned pairs | exercises |
| `03_branch_delay` | (current_pc, next_pc, delay_slot) window machine, branches/jumps, mult/div timing | exercises |
| `90_debug` | five seeded delay-slot bugs + `bug-report.md` | debug |
| `91_challenge` | headless runner over hand-assembled smoke fixture vs goldens | challenge |
| `99_coding_test` | unseen family: REGIMM link variants BLTZAL/BGEZAL | coding_test |

Fixtures and goldens: `tests/public/ch38_ps1_r3000a_cpu/`.
Hidden grading: `tests/hidden/ch38_ps1_r3000a_cpu/manifest.json`.

External conformance suites are referenced but never shipped:
[ps1-tests](https://github.com/avocado-ps1/ps1-tests) is wired as an
`optional`/`requires_rom` hidden case (`roms/ps1/ps1-tests/cpu_instr.bin`,
student-supplied; grading skips gracefully when absent).

## Verification

Recorded from the authoring pass:

```
VERIFY_PREFIX=/tmp/labs-PS1A tools/labs/verify_chapter.sh ch38_ps1_r3000a_cpu
  -> skeleton build OK (tests RED as expected)
  -> SOLUTIONS: GREEN — 100% tests passed out of 6
```

Hidden manifest validated by direct execution of the solution-tree runner:

| case | result |
|------|--------|
| hidden_fixture_ram_hash | exit 0, fnv64 file hash matches `4C512913D39CA4C6` |
| smoke_golden_trace | exit 0, trace fnv64 matches `4AF4887EA2F488BB`; solution run twice byte-identical |
| coding_test_regimm_link_variants | test binary exits 0 on solution build |
| ps1_tests_cpu_suite_optional | skipped (requires user-supplied ROM) |
