# Debugging exercise — the width-misread CPU

`buggy_exec.hpp` is the ch31 executor with **one seeded defect** on the
STUB side of `step()`'s `LDA #imm` handler. The SOLUTION side is the
correct reference behavior.

## Symptom

Programs that switch the accumulator to 8-bit and then use an
`LDA #imm8` go wrong:

- The immediate consumes **two** bytes instead of one, so every
  following instruction is fetched from the wrong address (the byte
  after the immediate gets eaten as operand padding).
- The hidden high byte `B` of the accumulator is clobbered with the
  second byte read, instead of being preserved.
- Traces diverge exactly at the `op=A9` line whenever flag M was set;
  before that instruction the trace matches the reference perfectly.

## Reproduce

```sh
python3 tools/labs/generate.py --force --targets ch31_snes_65c816_cpu
cmake --build build -j && ctest --test-dir build -R ch31_90_debug
```

Both tests fail on the skeleton. The first pins the register result,
the second pins the whole trace (first divergence = first `A9`
executed with M=1).

## Hint ladder

1. Which flag decides how many operand bytes `A9` fetches? Where in
   `step()` is that decision made?
2. Compare the STUB handler against a real W65C816 datasheet entry for
   `LDA #`: what does hardware do with register B when M=1?
3. After fixing the byte count, check the cycle count too — it must be
   2 in BOTH widths.

## Deliverable

Write `bug-report.md` next to your solution containing:

```text
bug:            <one sentence>
root cause:     <which line ignores which flag>
first observable divergence: <trace line / register write>
fix:            <diff description>
regression test:<name of the test you added or extended>
```
