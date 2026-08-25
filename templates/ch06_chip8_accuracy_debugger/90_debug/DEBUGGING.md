# DEBUGGING — ch06 seeded compatibility bug

## Symptom

A small save/reload program (`kSaveReload` in `main.cpp`, and committed as
`tests/public/ch06_chip8_accuracy_debugger/fixtures/save_reload.bin`) was
assembled for a **COSMAC VIP**. Under `--quirks COSMAC_VIP` it misbehaves:

```bash
./ch06_90_debug_runner \
    --rom tests/public/ch06_chip8_accuracy_debugger/fixtures/save_reload.bin \
    --quirks COSMAC_VIP --cycles 6 --trace /tmp/actual.log --trace-full
python3 tools/labs/compare_trace.py \
    tests/public/ch06_chip8_accuracy_debugger/traces/save_reload_vip.full.log \
    /tmp/actual.log
```

The trace diverges right after the reload: V3 loses its saved value, the
subsequent `SE V3, 0x12` fails to skip, and the symptom shows up one
instruction later as `V0=FF`. The same ROM runs fine under MODERN — which
is what makes this bug sneaky: the emulator looks "mostly right".

## Your task

Produce a `bug-report.md` containing exactly five sections:

1. **bug** — one sentence describing WHAT is observably wrong.
2. **root cause** — which handler in `chip8.hpp` is wrong and why.
3. **first observable divergence** — the pc= of the earliest trace line that
   contradicts the golden trace (trace-first debugging, §54).
4. **fix** — the corrected code (one condition).
5. **regression test** — the test you would add so this never comes back.

Then fix the bug and make `ch06_90_debug_tests` pass. Note how lopsided the
effort is once the trace pointed at the right line — that is the lesson.
