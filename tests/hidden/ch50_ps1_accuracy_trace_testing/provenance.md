# ch50 hidden manifest provenance

`manifest.json` — 16 cases, 15 executable + 1 optional.

## Case inventory and expected results (solution build)

Ten regression cases, one per seeded bug; each runs a single labstest
filter against `90_debug/ch50_90_regress_tests`:

| case                                  | filter  |
|---------------------------------------|---------|
| regression_seed01_addi_sign_extend    | seed01  |
| regression_seed02_trace_entry_pc      | seed02  |
| regression_seed03_fill_width          | seed03  |
| regression_seed04_blit_stride         | seed04  |
| regression_seed05_env_quantum         | seed05  |
| regression_seed06_dma_count           | seed06  |
| regression_seed07_dma_chain_enable    | seed07  |
| regression_seed08_gte_shift_order     | seed08  |
| regression_seed09_timer_reload        | seed09  |
| regression_seed10_cdrom_bcd           | seed10  |

Plus the aggregate and unit cases:

| case                              | binary            | args                                        |
|-----------------------------------|-------------------|---------------------------------------------|
| aggregate_full_suite_manifest     | 01_suite_runner/ch50_01_accuracy_runner | `tests/public/ch50_ps1_accuracy_trace_testing/suites/suite.txt` |
| suite_runner_units_parse          | 01_suite_runner/ch50_01_runner_tests | `suite_parse`    |
| suite_runner_units_builtins       | 01_suite_runner/ch50_01_runner_tests | `builtin_checks` |
| suite_runner_units_report         | 01_suite_runner/ch50_01_runner_tests | `report`         |
| challenge_aggregate_in_process    | 91_challenge/ch50_91_challenge_tests | (none)           |

All expect exit 0. On the skeleton build the ten regression cases fail RED
(one per seeded bug) by design; on the solution build all pass.

The aggregate case intentionally exercises the PUBLIC suite file: it runs
the seven built-in checks plus the same ten regression filters through the
runner subprocess mechanism, so grading covers the runner's spawn/exit
path too.

## Optional hardware-suite gate

`psx_tests_cpu_exception_optional`: gated on the student-supplied external
suite binary at `roms/ps1/psx-tests/cpu_exception.bin` (community ps1-tests
ecosystem, https://github.com/JaCzekanski/ps1-tests — referenced by URL
only; never committed here). When the ROM is absent the grader SKIPS the
case without failing the chapter. When present, it currently runs the
runner's built-in suite: this chapter's psx-mini tier cannot execute real
PS-X EXE images yet; the case exists to pin the gating pattern and will be
wired to real EXE loading as the CPU chapters mature.

## Execution record

All 15 executable cases were executed with the exact manifest arguments
against scratch-built solution binaries via tools/labs/grade.py:
16/16 passed (15 PASS + 1 SKIP for absent ROM). Goldens were generated
twice from the reference solution and compared byte-identical (`cmp`).
