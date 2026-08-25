# Unseen System Test — ch52

The unseen fixture is a tiny NROM homebrew with a behavior contract that is
NOT practiced by the committed fixtures: it polls **sprite 0 hit** to split
its frame, reads $4016 once per vblank, and colors scanline bands according
to the button bits.

Your composed machine must, over 120 frames with the committed input
script:

1. reach the sprite-0 wait loop (observable via CPU trace pc range),
2. produce both band colors in the framebuffer (two distinct non-zero
   shades above and below the split line),
3. mirror button state into RAM $0200..$0203 each frame,
4. emit a pulse tone on frames 8..64 only.

Grading runs your `LABS_NES_GATE_BIN` runner against
`tests/hidden/ch52_nes_playable_gate/roms/unseen_gate.nes` with:

```text
--input-file tests/hidden/ch52_nes_playable_gate/scripts/gate.input
--frames 120 --hash-frame {{tmp}}/f.rgba --audio-out {{tmp}}/a.pcm
```

and pins FNV64 hashes of both outputs plus the exit code. Implement the
documented behavior; do not special-case the ROM.
