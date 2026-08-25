# CODING TEST — ch30: isolate a failing subsystem without a GUI

Curriculum milestone: the reference core ships a **headless deterministic
test runner**. This chapter's is `ch30_suite_runner`. It executes named
subsystem suites (audio / saves / eeprom) with fixed inputs, prints
`[PASS|FAIL] suite=<name> digest=<FNV-64>` per suite, and exits non-zero
when any executed suite fails.

## The workflow being drilled

A full-system run fails. You may not open a GUI (there isn't one). Instead:

```bash
ch30_suite_runner                      # run everything: which suites fail?
ch30_suite_runner --suite saves        # re-run only the failing subsystem
ch30_suite_runner --isolate audio     # everything except audio: does the
                                      # failure follow the subsystem?
```

If `--suite saves` fails while `--isolate saves` passes, the fault lives in
the saves subsystem itself; if both fail identically, look at shared state.

## Your deliverable (already provided here)

The runner and its three suites are implemented; the hidden grader runs:

1. `--list-suites` output contains all three names,
2. each individual `--suite` run exits 0 on the solution tree,
3. a conformance test binary (`ch30_coding_tests`) validating the contract:
   filtering changes WHICH suites execute, never their digests.

Digests are deterministic: same code + inputs => same digest, any machine.
