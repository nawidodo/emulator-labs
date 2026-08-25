# DEBUGGING — registers are being corrupted

## Reported symptom

A student's CHIP-8 probe program (`roms/debug_probe.ch8`) is supposed to end
with `VA = 0x42` and `VB = 0x03`, then park in a self-jump. Instead, the
registers land in the wrong places. The program "runs fine" — no crash, the
PC parks exactly where expected — but the state is wrong.

## Reproduce

```bash
./ch03_90_debug_runner \
    --rom templates/ch03_chip8_architecture/90_debug/roms/debug_probe.ch8 \
    --headless --cycles 6 --trace /tmp/buggy.log
cat /tmp/buggy.log
```

Symptom trace excerpt (buggy build; middle registers elided for print):

```text
pc=0204 op=6A42 V2=42 VA=00 ... I=000 SP=00 DT=00 ST=00 cyc=2
pc=0206 op=7B03 V3=03 VB=00 ... I=000 SP=00 DT=00 ST=00 cyc=3
```

Expected (reference solution, same command — also committed as
`tests/public/ch03_chip8_architecture/traces/debug_probe_golden.log`):

```text
pc=0204 op=6A42 V2=00 VA=42 ... I=000 SP=00 DT=00 ST=00 cyc=2
pc=0206 op=7B03 V3=00 VB=03 ... I=000 SP=00 DT=00 ST=00 cyc=3
pc=0208 op=A208 V2=00 VA=42 VB=03 ... I=208 SP=00 DT=00 ST=00 cyc=4
```

Produce `bug-report.md` with exactly these five sections:

```text
bug
root cause
first observable divergence
fix
regression test
```

Rules:

- Do not stare at the screen; diff traces. Run your build and the golden
  trace through `tools/labs/compare_trace.py` and find the **first**
  divergence — not the last visible symptom.
- The first divergence in the excerpt above happens at `cyc=2` on opcode
  `6A42`. Explain what the decoder did with that word and why it produced
  `V2` instead of `VA`.
- Fix the root cause (one line), not the symptom. No special-casing
  opcodes; the field extractor must be correct for every opcode at once.
- Keep the tests in `main.cpp` green afterwards; they are the regression
  test. If you needed a new test to expose the bug before fixing, add it.

## Hint ladder

1. Which nibbles of `0x6A42` are `X` supposed to come from? Write out the
   bits: `0110 1010 0100 0010`.
2. Where does the buggy code read them from?
3. Why do `ANNN` and `1NNN` still work perfectly while every `6XNN`/`7XNN`
   misbehaves?
