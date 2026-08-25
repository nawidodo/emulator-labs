# CHALLENGE — ch28

## Part A — full-scene golden frame (graded)

Compose one frame of a mixed scene — text background, affine background,
sprites (one semi-transparent), a window and alpha blending — using your
`05_compositor` pipeline. `main.cpp` here builds the exact scene and checks:

1. rendering it twice yields identical bytes (determinism), and
2. the FNV-64 digest matches the committed golden in
   `tests/public/ch28_gba_ppu/frames/challenge_frame.rgba` (see
   `provenance.md` there for how it was produced).

The scene exercises every chapter feature at once; any bug in priority,
blending, affine wrap or sprite fetch changes the digest.

Run locally:

```bash
ctest --test-dir build -R ch28_91_challenge --output-on-failure
```

## Part B — mGBA graphics suite (optional hardware gate)

The [mGBA suite](https://github.com/mgba-io/suite) ships GBA ROM images that
exercise the real PPU (affine matrices, priorities, windows, mosaic). This
repository never carries commercial or test ROM binaries, so the hidden
manifest gates these behind `requires_rom` + `optional`: if you drop e.g.
`suite.gba` into `roms/gba/mgba-suite/`, grading runs it; otherwise the case
skips gracefully.

Suggested acceptance before declaring your PPU "done":

```text
affine*              all pass
priority*            all pass
window*              all pass
mosaic*              all pass
```

Document any failures with trace captures — trace-first debugging applies
to graphics too (dump per-scanline layer decisions).
