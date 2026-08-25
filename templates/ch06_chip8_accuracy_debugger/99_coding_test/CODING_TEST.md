# CODING TEST — ch06: the wrong-profile mystery

You are handed a ROM and one trace excerpt, nothing else. The excerpt below
was produced by the reference emulator running `mystery.bin` — but with the
**WRONG quirk profile**, which is why the program "fails" on real hardware.

Your job (curriculum ch6 coding test): *given a failing ROM plus an
execution trace, find and fix the compatibility bug.*

1. The ROM lives at `tests/hidden/ch06_chip8_accuracy_debugger/roms/mystery.bin`
   (its disassembly is next to it). Field notes say it came off a **POTATO-48
   emulator disk image** — an early CHIP-48-family machine. Reproduce the
   failure:

   ```bash
   ./ch06_01_trace_runner \
       --rom tests/hidden/ch06_chip8_accuracy_debugger/roms/mystery.bin \
       --cycles 8 --trace /tmp/mine.log --trace-full
   ```

2. Compare against the excerpt. Identify which historical behavior each
   diverging field implies (shift source? I after FX55? jump base? VF reset?
   edge wrap?) — every line of the trace is evidence. The five fixtures in
   exercise 04 are your Rosetta stone: run them under candidate profiles
   until their fingerprints match the symptom pattern.

3. Re-run with `--quirks <PROFILE>` using your conclusion. When you have
   found the right profile, the full-mode trace hash matches the golden:
   that automated check is the graded part (`coding_test_mystery_profile`
   in the hidden manifest).

## The shipped (wrong-profile) trace excerpt

```text
pc=0200 op=62F0 ... V2=00 V3=00 VF=00 I=000 cyc=0
pc=0204 op=8236 ... V2=F0 V3=01 VF=00 I=000 cyc=2
pc=0206 op=8420 ... V2=78 V3=01 VF=00 V4=00 cyc=3
pc=020C op=F355 ... V3=12 VF=00 I=400 cyc=6
pc=020E op=F365 ... V3=12 VF=00 I=404 cyc=7
```

(Fields not relevant to the hunt are elided; run it yourself for the full
lines.)

## What "solved" looks like

- Your runner, given the correct profile flag, produces a byte-identical
  trace to the reference solution's CHIP48-era output.
- You can say WHY in one sentence per opcode family.
