# EMU_GATE_V1 — whole-machine checkpoint format (course reference)

Version 1 checkpoint payload produced by the canonical reference machines
(`tools/labs/nes_gate/nes_gate_runner`) and consumed by the grader for
`expect_file_hash`-style pinning of integration gates.

## Layout

One checkpoint per run, written to the `--gate` output file. Plain text,
UTF-8, LF line endings, `KEY=VALUE` lines; order is not significant;
unknown keys MUST be ignored by consumers; a missing required key makes
the checkpoint INVALID.

```
EMU_GATE_V1
ROM_FNV=XXXXXXXXXXXXXXXX
FRAME=<N>
CPU_PC=XXXX
CPU_A=XX
CPU_X=XX
CPU_Y=XX
CPU_SP=XX
CPU_P=XX
CPU_CYC=<decimal>
RAM_FNV=XXXXXXXXXXXXXXXX
PPU_FNV=XXXXXXXXXXXXXXXX
FRAME_FNV=XXXXXXXXXXXXXXXX
AUDIO_FNV=XXXXXXXXXXXXXXXX
REPLAY_FNV=XXXXXXXXXXXXXXXX
```

## Fields

| Key          | Meaning                                                        |
|--------------|----------------------------------------------------------------|
| `ROM_FNV`    | FNV-1a 64 (uppercase hex, `%016X`) over the raw ROM bytes      |
| `FRAME`      | 1-based index of the frame that produced `FRAME_FNV`           |
| `CPU_PC/A/X/Y/SP/P` | final CPU registers at the last cycle of that frame     |
| `CPU_CYC`    | total CPU cycles executed up to that frame end                 |
| `RAM_FNV`    | FNV-1a 64 over the 2 KiB CPU RAM ($0000-$07FF) at frame end    |
| `PPU_FNV`    | FNV-1a 64 over PPU state: ctrl, mask, v (16-bit), t, x, w,     |
|              | scanline, dot, then VRAM (2 KiB), palette (32 B), OAM (256 B)  |
| `FRAME_FNV`  | FNV-1a 64 over the raw 256x240x4 RGBA8 frame output            |
| `AUDIO_FNV`  | FNV-1a 64 over the mono s16le PCM segment (one sample per CPU  |
|              | cycle, per ch24 contract) produced during that frame           |
| `REPLAY_FNV` | FNV-1a 64 over the deterministic run with identical input;     |
|              | must equal `FRAME_FNV` when replay self-check is enabled       |

FNV-1a 64: offset basis `0xCBF29CE484222325`, prime `0x100000001B3`,
byte-at-a-time, same constants as `tools/labs/hash_frame.py` and
`nes21fix::fnv1a64`.

## Provenance

- Written by the canonical reference runner in `tools/labs/nes_gate/`,
  composed from the verified ch18-ch24 course components.
- Goldens are minted by running the reference twice and requiring
  byte-identical checkpoints (see `tests/public/ch52_nes_playable_gate/
  goldens/provenance.md`).
