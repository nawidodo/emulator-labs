# Coding test — ch21: render an unseen nametable snapshot

At grading time you receive a `.nesf` snapshot you have never seen: an
unseen nametable arrangement, attribute layout, palette, and mirroring
mode. Your Chapter 21 runner must render it byte-identically to the
reference.

## What is tested

- PPU address-space decode incl. `$3000-$3EFF` folding and the fixture's
  mirroring mode (your renderer resolves the `$2000` window through your
  `PpuBus`).
- Attribute quadrant selection across the whole frame.
- Planar pattern decoding from both CHR halves.
- Backdrop rule for tile color 0.

## Procedure

```bash
# hidden fixture path is announced by the grader; locally rehearse with:
./ch21_03_render_runner --rom <fixture>.nesf --frames 1 --headless \
    --hash-frame /tmp/unseen.rgba
python3 tools/labs/hash_frame.py /tmp/unseen.rgba --fnv-only
```

The grader compares FNV-1a 64 of the raw RGBA bytes against a reference
value generated from the same fixture. No partial credit: one wrong pixel
is a wrong emulator. Rehearse with the public scenes in
`tests/public/ch21_nes_ppu1/fixtures/` — they cover every rule above except
the exact hidden arrangement.
