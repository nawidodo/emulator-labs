# CI verification & grading diagnostics

This document records the CI troubleshooting guidance and grading/manifest expectations for emulator-labs. It is intended to help diagnose stuck CI runs (the live verification gate), add new chapter templates so CI builds them, and reproduce hidden/unseen grader failures locally.

## 1) Diagnosing a stuck "Live verification gate" (verify_current.py)

What the step does

- The CI `ci` workflow runs `verify_current.py` toward the end of the ubuntu/gcc matrix. `verify_current.py` runs subprocesses and emits `verification-current.json` summarizing evidence dimensions:
  - `reference_ctest` — runs `ctest` on `build-solutions`
  - `unseen_grade` — runs `tools/labs/grade.py --allow-missing-env`
  - `pipeline_grade` — when `--pipeline` is passed
  - `grader_self_tests` — runs `python -m unittest discover -s tools/labs/tests`
  - `foundations-c17` audit — runs `tools/labs/audit_track.py`

What to inspect in the job logs/artifacts (in order)

1. CTest step console log ("CTest (all chapters must be GREEN in solutions mode)")
   - Look for the ctest summary line like: `100% tests passed, 0 tests failed out of N` or the regex used by the repo: `(\d+)% tests passed(?:,\s*(\d+) tests failed)? out of (\d+)`.
   - If ctest is hanging you will see the last per-test output and no final summary.
2. Hidden/unseen grade sweep console log ("Hidden/unseen grade sweep vs solution binaries")
   - `grade.py` prints per-chapter sections (`=== grade <chapter>:`) and case lines such as:
     `  [PASS] case_name: ok` or `  [FAIL] case_name: reason`.
   - The final summary line is: `== grade summary: X passed / Y skipped / Z failed (of N) ==`.
   - If the sweep stalls, note the last printed case and inspect `.labs/failures/` for that case.
3. Grader self-tests console (unittest)
   - Look for `Ran N tests` and `OK` or `FAIL` messages.
4. verify_current.py console output
   - `verify_current.py` prints the full JSON object before exiting. Check `reference_ctest`, `unseen_grade`, `grader_self_tests`, `foundations-c17` fields and the `overall` status.
   - If the script is hanging, the console will show which subprocess it’s waiting on (ctest, grade.py, or audit_track.py).
5. Downloaded artifact: `verification-current` (artifact `verification-current.json`)
   - Download and open this JSON to read parsed statuses and `reason` fields for invalid or not-run dimensions.
6. Grade failure artifacts on disk
   - `grade.py` writes diagnostics under `.labs/failures/<chapter>/<case>/` with `reason.txt`, `stdout.txt`, `stderr.txt` and any produced files. Inspect these for case-level details.
7. `.labs/grade-last.json`
   - The grade summary is recorded here; use it to locate failing cases programmatically.

Commands to fetch logs/artifacts

- GitHub CLI (recommended):
  - `gh run view 32923789224 -R nawidodo/emulator-labs --log`
  - `gh run download 32923789224 -R nawidodo/emulator-labs`  # includes artifacts
- Actions UI: open the run, inspect step console logs and download Artifacts → `verification-current`
- API/curl: use the Actions runs and artifacts endpoints with a token if CLI/UI is not available.

Quick checklist of JSON/log fields to verify

- `reference_ctest.status` should be `green`. If `error`/`not-run`, read `reason`.
- `unseen_grade.status` should be `green`. If `error`/`invalid`, inspect `.labs/failures` and the grade console output.
- `grader_self_tests.status` should be `green` and show `Ran N tests`.
- `foundations_c17_audit` should be `green` or have an `reason` field explaining failure.
- `overall` will be `green` / `incomplete` / `red`. `verify_current.py` exits nonzero when required dimensions are not green.

Common hang sources & quick mitigations

- ctest or a test binary is interactive or blocked: ensure tests are non-interactive and add timeouts where appropriate.
- grade.py launched a student integration binary that blocks (waiting for input or missing ROM): inspect `.labs/failures` and increase the case `timeout` if necessary while debugging.
- audit_track.py or C17 compile may be long on constrained runners: consider using larger runners or reducing the per-job workload.
- Network/download delays (apt/docker pulls) may cause earlier steps to be slow/stall; inspect earlier build logs for long download lines.


## 2) Adding a new chapter template so root CMake/CI builds it

Minimal files to add under `templates/ch52_nes_playable_gate/`:

- `templates/ch52_nes_playable_gate/README.md` (chapter description)
- `templates/ch52_nes_playable_gate/LECTURE.md` (optional)
- One or more exercise subdirectories, each containing:
  - `templates/ch52_nes_playable_gate/01_name/CMakeLists.txt`
  - `main.cpp` / test sources and any test fixtures
  - A `CMakeLists.txt` that declares an executable, links `labstest` and calls `add_test(...)`

Minimal example CMakeLists fragment for an exercise:

```cmake
add_executable(ch52_01_runner main.cpp)
target_link_libraries(ch52_01_runner PRIVATE labstest)
add_test(NAME ch52_01 COMMAND ch52_01_runner)
```

Other things to add for CI integration

- If you want public fixtures or grader cases, add `tests/public/ch52/...` and/or `tests/hidden/ch52/manifest.json`.
  - `tests/hidden/ch52/manifest.json` enables the unseen grader sweep for the chapter.
- Update `course-manifest.json` only if your manifest-driven tooling requires explicit chapter metadata (audit_manifest.py may expect certain fields).

Why CI will build it

- CI runs `python3 tools/labs/generate.py --mode solution --force --targets all`, which copies/expands `templates/` into a `solutions/` tree. The root `CMakeLists.txt` globs for `${LABS_ROOT}/ch*/CMakeLists.txt` and `${LABS_ROOT}/ch*/*/CMakeLists.txt` and runs `add_subdirectory` for each found directory. As long as `generate.py` produces `solutions/ch52/.../CMakeLists.txt` and tests, CI will configure, build and run them.


## 3) What `tools/labs/grade.py` expects from `tests/hidden` manifests and how to reproduce failures locally

Manifest shape (summary)

- Path: `tests/hidden/<chapter>/manifest.json`
- JSON shape:
  - `description`: string
  - `cases`: array of case objects. Per-case fields supported:
    - `name`: string
    - `binary`: path to executable (supports placeholders `{{tmp}}` and `{{env:NAME}}`)
    - `args`: array of strings
    - `expect_exit`: int
    - `expect_stdout_contains`: substring
    - `expect_file_exists`: [paths]
    - `expect_file_hash`: { "file": "<path>", "fnv64": "<HEX>" }
    - `requires_rom`: path to a student-supplied ROM (used in combination with `optional`)
    - `optional`: boolean
    - `timeout`: seconds (default 30)
    - `required_env`: boolean (if true and binary is unset, the case should fail unless `--allow-missing-env`)

How grading works and where results land

- `grade.py` expands placeholders, runs each case command, enforces timeouts, and validates return code, stdout contents, produced files, and FNV64 file hashes.
- On failure it persists diagnostics under `<repo>/.labs/failures/<chapter>/<case>/` with `reason.txt`, `stdout.txt`, `stderr.txt`, and any produced artifacts.
- It writes a final `.labs/grade-last.json` and prints a summary: `== grade summary: X passed / Y skipped / Z failed (of N) ==`.
- Exit status is nonzero if any non-skipped chapters failed.

Reproducing a failing hidden case locally

1. Run the grader locally for the chapter or all hidden tests:
   - Single chapter: `python3 tools/labs/grade.py --repo . ch02_emulator_core`
   - Full unseen sweep: `python3 tools/labs/grade.py --repo .`
   - Author sweep (skip missing integration binaries): `python3 tools/labs/grade.py --repo . --allow-missing-env`
2. When a case fails, inspect: `.labs/failures/<chapter>/<case>/reason.txt`, `stdout.txt`, `stderr.txt` and any produced files.
3. Re-run the exact command manually using the expanded `binary` and `args` (expand `{{tmp}}` to a temp dir and `{{env:NAME}}` to the env var value). Example:
   - `mkdir /tmp/labs-debug`
   - `build/skels/.../runner --rom tests/hidden/ch02/roms/prog.bin --cycles 500 --trace /tmp/labs-debug/t.log`
4. If a file hash mismatch occurs, compute FNV1a of the produced file and compare to the manifest’s `fnv64` value. The grader uses FNV-1a 64-bit (hex uppercase).

Notes about timeouts and blocking

- Default case timeout is 30s. If a binary blocks (waiting for input), the case may time out; increase `timeout` in the manifest for debugging or fix the binary to be non-interactive.


---

If you want, I can now commit this file to the repository at `docs/CI_VERIFY.md` with the message you provided. Reply `yes` to proceed and I will push it (I already have the path and commit message you requested).