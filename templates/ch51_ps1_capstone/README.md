# ch51 — PS1 Full Emulator Capstone

**No starter implementation ships with this chapter.**

Everything from the PS1 phase now exists as components you wrote and
verified across chapters 38–50. The capstone deliverable is a single,
integrated emulator binary that composes them into one machine and passes
hidden milestone ROMs. This README is the contract for that integration.

## Architecture

```
                    ┌────────────┐
                    │   R3000A   │
                    └──────┬─────┘
                           │
                      System Bus
                           │
        ┌──────────┬───────┼────────┬───────────┐
        ↓          ↓       ↓        ↓           ↓
       RAM        GPU     DMA      SPU        CD-ROM
                   │       │
                   │       ├──── GPU
                   │       ├──── SPU
                   │       ├──── CD
                   │       └──── MDEC
                   │
                  GTE
```

The scheduler (ch49) owns all time: nothing advances except through
scheduler events, and every interrupt reaches the CPU through the INTC in
a deterministic order.

## Integration deliverable checklist

Your integrated emulator must contain **all** of the following. Each row
names the chapter whose verified implementation is the expected source;
reusing your own solutions there is the intended path.

| # | Component            | Source chapter | Acceptance evidence                                  |
|---|----------------------|----------------|------------------------------------------------------|
| 1 | R3000A CPU           | ch38           | CPU trace tests (`pc=<hex> op=<hex> ... cyc=<n>`)    |
| 2 | COP0 + exceptions    | ch39           | exception vector tests                               |
| 3 | Memory control/map   | ch39           | RAM/BIOS mapping + scratchpad state tests            |
| 4 | INTC                 | ch40           | interrupt latch/ordering tests                       |
| 5 | Timers               | ch40           | timer IRQ/sync mode tests                            |
| 6 | GPU commands + VRAM  | ch41/42        | VRAM-hash corpus                                     |
| 7 | DMA fabric           | ch43           | chain-list transfer state tests                      |
| 8 | GTE                  | ch44           | GTE regression vectors                               |
| 9 | CD-ROM               | ch45           | command/state tests incl. latency model              |
|10 | MDEC                 | ch46           | decoded-block hash corpus                            |
|11 | SPU                  | ch47           | sample-hash corpus                                   |
|12 | Controller           | ch48           | digital pad transaction log replay                   |
|13 | Memory card          | ch48           | card protocol round-trip (.mcr image)                |
|14 | Scheduler            | ch49           | event-log hash; no ad-hoc stepping anywhere          |
|15 | Debugger             | ch36           | headless trace/dump CLI works on integrated binary   |
|16 | Save states          | ch35           | save -> load -> continue reproduces identical hashes |
|17 | Headless tests       | ch50           | your accuracy suites pass against the integration    |

Rows 1–14 are graded by the hidden capstone manifest; rows 15–17 are
self-certified through your own repo layout and documented below.

## Grading flow (external binary via environment variable)

This chapter has no buildable template target — the thing being graded is
*your* integrated binary living in *your* repository. Hidden manifests
therefore reference it through an environment-variable placeholder:

```json
{
  "name": "capstone_pad_transaction",
  "binary": "{{env:LABS_CAPSTONE_BIN}}",
  "args": ["--rom", "tests/hidden/ch51_ps1_capstone/roms/pad_txn.bin",
           "--input-file", "tests/hidden/ch51_ps1_capstone/scripts/pad.script",
           "--hash-frame", "{{tmp}}/resp.bin", "--headless"],
  "expect_file_hash": {"file": "{{tmp}}/resp.bin", "fnv64": "..."}
}
```

`tools/labs/grade.py` expands `{{env:NAME}}` in the `binary`, `args` and
`expect_file_hash.file` fields before executing. Note: with the variable
**unset**, expansion produces an empty path that resolves to the repo
directory and the grader aborts with a permission error — always set it.
To be graded:
1. Build your integrated emulator.
2. Point the variable at it, e.g.
   `LABS_CAPSTONE_BIN=$HOME/my-psx/build/psx_emu python3 tools/labs/grade.py --repo . ch51_ps1_capstone`
3. Your binary MUST implement the standard runner CLI (curriculum §52):
   `--rom PATH --headless --cycles N --frames N --trace FILE --hash-frame FILE --input-file FILE`
   with deterministic output — same input, byte-identical dumps, always.
4. Manifest cases run protocol/state checks against that binary:
   pad/card transactions, DMA/GTE/timer/CDROM state pins, VRAM and SPU
   hashes, and scheduler event-log hashes from synthetic fixture ROMs.

See `99_coding_test/CODING_TEST.md` and
`99_coding_test/manifest.example.json` for the full case catalogue.

## Out-of-the-box self-check

One hidden case runs without any student binary: it validates the grading
pipeline itself (executable resolution, `{{tmp}}` scratch dirs, FNV-64
file hashing) using a committed deterministic script under
`tests/hidden/ch51_ps1_capstone/selfcheck/`. It must stay GREEN at all
times; if it fails, the grading environment — not a student — is broken.

## Gate checklist

- [ ] starter: none by design (this chapter IS the final integration)
- [ ] exercises/debug/challenge: subsumed by the checklist above
- [ ] coding_test: `LABS_CAPSTONE_BIN=... make grade GRADE_TARGETS=ch51_ps1_capstone`
- [ ] self-check: `python3 tools/labs/grade.py --repo . ch51_ps1_capstone`
      passes its pipeline self-check case on any machine

## Boot milestones (comprehensive review #25)

The integrated machine must demonstrate, in order:

```text
reset PC / BIOS mapping  ->  synthetic BIOS stub runs
      ->  initialized machine  ->  PS-X EXE / homebrew executes
      ->  (stretch) synthetic disc boot path
```

Course-original BIOS stub and homebrew disc only — no Sony BIOS, no
commercial games.

## Accuracy integration (review #24)

Layer B: apply an accuracy-suite contract to YOUR integrated binary.
Point the environment variable at it and run the suite filters against
your build (the psx-mini contract in ch50 defines the register-level
interface your binary must expose):

```bash
LABS_PS1_BIN=/path/to/your/psx make grade GRADE_TARGETS=ch50_ps1_accuracy_trace_testing
```

## Final explain gate (from the C17 foundations track)

Before calling the capstone done, write out — from memory, no notes — the
three end-to-end traces. If a link is fuzzy, that is the subsystem to
re-study:

```text
host button  -> C API input state -> SIO/controller protocol -> CPU MMIO read
             -> R3000A game logic -> GTE/GPU state -> VRAM pixel
             -> host framebuffer

CPU writes SPU register -> voice/ADSR timers -> channel sample -> mixer
             -> PCM FIFO -> host audio backend

CPU consumes cycles -> master emulated time -> GPU/SPU/DMA/CD/timers and
interrupt lines advance deterministically
```

An emulator you cannot explain is one you cannot debug.

## Verification

ch51 has no buildable template by design; verification is:

```bash
# 1) the out-of-the-box self-check case passes:
python3 tools/labs/grade.py --repo . ch51_ps1_capstone
# 2) the env-gated example flow was exercised manually with a stub
#    binary placed at $LABS_CAPSTONE_BIN (see provenance.md).
```

Recorded results live in `tests/hidden/ch51_ps1_capstone/provenance.md`.
