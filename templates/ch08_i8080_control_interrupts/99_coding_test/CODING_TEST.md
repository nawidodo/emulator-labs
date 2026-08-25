# Chapter 8 Coding Test — First-Divergence Hunter

Given two execution traces of the same program (the committed golden and a
buggy run), report the **first divergence**: the earliest trace line where
they disagree. This is the core skill of curriculum §54 — find the first
incorrect instruction, not the last visible symptom.

## Interface

`diverge.hpp` in namespace `labsdiv`:

| Function | Contract |
|---|---|
| `parse_line(line, out)` | split whitespace-separated `key=value` tokens; `false` for tokens without `=` or empty parses |
| `parse_trace(in)` | all non-blank lines as field maps (blank lines skipped so trailing newlines don't shift alignment) |
| `rows_equal(a, b)` | exact key+value set equality |
| `first_divergence(golden, actual)` | 1-based index of the earliest mismatched row; a length mismatch diverges at the first extra row; identical traces give 0 |

## Hidden grader

The grader runs:

```bash
i8080_divergence_tests --check GOLDEN ACTUAL EXPECTED_INDEX
```

against committed trace pairs under `tests/hidden/ch08_i8080_control_interrupts/traces/`.
Exit 0 is required. The same binary runs its own suites when invoked
without arguments.

## Traps

- A trace that merely TRUNCATES still has a divergence (at the cut).
- Blank lines must not shift row indices.
- Field ORDER doesn't matter; the key=value SET does.
