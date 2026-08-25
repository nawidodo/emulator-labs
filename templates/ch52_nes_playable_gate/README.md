# ch52 — Playable NROM Gate (executable prerequisite for the PS1 phase)

This chapter is the **whole-machine gate** from the review/foundations
tracks: a legally-distributable-style NROM homebrew must BOOT, respond to
CONTROLLER input, produce VIDEO, produce BASIC AUDIO, and REPLAY
deterministically — headlessly — before the PS1 phase unlocks.

## Why there is no starter implementation

Like `ch51_ps1_capstone`, this chapter defines an INTEGRATION deliverable,
not new subsystem code. You compose the machine from your own ch18–ch24
implementations:

```text
CPU (ch18/ch19)  ─┐
bus + RAM mirror  │  (ch20)
NROM cart         │  (ch20)
PPU bg/sprites    │  (ch21/ch22)      ──> frame hashes
controller        │  (ch20 shift reg) <── scripted input file
OAM DMA           │  (ch24)
APU pulse path    │  (ch24)           ──> PCM segment hash
scheduler CPU:PPU=1:3                  (ch24)
```

## Gate checklist

- [ ] boot: committed course-original NROM homebrew boots; success code
      observable in RAM / VRAM (`91_playable_nrom` acceptance)
- [ ] controller: strobe/latch/shift read of $4016 matches the scripted
      input stream, bit order and per-frame latching included
- [ ] video: full-frame RGBA hash of the running homebrew after N frames
- [ ] audio: pulse-channel PCM segment hash over the same run
- [ ] replay: record the input stream, replay it, frame+audio hashes
      identical (determinism proof)
- [ ] headless: every check above runs with no window/device

Explicitly NOT required to unlock PS1: mappers beyond NROM, DMC accuracy,
PPU race conditions, rewind, run-ahead, netplay, shaders, GUI.

## Grading flow

The hidden manifest executes against binaries you build from YOUR composed
machine. Point the environment variable at your runner:

```bash
LABS_NES_GATE_BIN=/path/to/your/nes_gate_runner \
  make grade GRADE_TARGETS=ch52_nes_playable_gate
```

`99_unseen_system_test/CODING_TEST.md` defines the unseen behavior
contract; its cases use the same env indirection. The always-on manifest in
`tests/hidden/ch52_nes_playable_gate/manifest.json` carries one
out-of-the-box self-check that passes on any machine.
