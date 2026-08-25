# CI failure: ch27_91_calc segfault

Short answer: the CI failure is caused by a segfault in the ch27_91_calc test — calc_runner.cpp calls fclose(nullptr) when writing to stdout. Fix: only close the FILE* when it's not stdout.

## What I did

- Fetched recent workflow runs for `nawidodo/emulator-labs` and inspected the failing run.
  - Failed workflow run: https://github.com/nawidodo/emulator-labs/actions/runs/32863285007
  - Listing used: https://github.com/nawidodo/emulator-labs/actions/runs?per_page=5 (showing 5 of 17 runs)
- Downloaded the failing job logs and found one test failing with a SEGFAULT:
  - 99% tests passed, 1 failed
  - Failing test: `239 - ch27_91_calc (SEGFAULT)`
  - Job terminated with "Process completed with exit code 8"
- Located the test runner and sources:
  - CMake sets up `ch27_91_calc` to run `ch27_calc_runner` (templates/ch27_gba_memory_system/91_challenge/CMakeLists.txt)
  - Runner source: `templates/ch27_gba_memory_system/91_challenge/calc_runner.cpp`
- Inspected `calc_runner.cpp` and found the bug: it calls `fclose(nullptr)` when `out == stdout`, which can cause a crash.

## Exact problematic code

Current (buggy) line in `calc_runner.cpp`:

```c++
std::fclose(out == stdout ? nullptr : out);
```

## Why this is the problem

- When no output file is given the code sets `out = stdout`. Then the expression `out == stdout ? nullptr : out` passes `nullptr` to `fclose`, which is undefined behavior and on the CI runner causes a crash (segfault) during the test process.

## Proposed fix

Replace the `fclose` call with a conditional close that only closes non-`stdout` files.

Suggested change:

```c++
if (out != stdout) std::fclose(out);
```

Or more verbosely:

```c++
if (out != stdout) {
    std::fclose(out);
}
```

## Next steps / options

I can apply this fix directly to the repository. Tell me which you prefer:
- Commit the change directly to the default branch (create a commit).
- Create a branch and open a pull request with the fix.
- I can just leave the patch here and you can apply it yourself.

If you want me to commit, tell me which option and I'll proceed.
