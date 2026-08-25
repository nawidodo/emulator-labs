# Coding test — ch22: diagnose screenshot diffs

A broken emulator produces screenshots that ALMOST match the reference.
You get two raw frames (256x240 RGBA8, no PNG — raw bytes diffed by hash)
and must report exactly where and how they diverge.

## The tool you build

`ch22_99_frame_diff FRAME_A.rgba FRAME_B.rgba` prints:

```text
hash_a=<FNV64 of frame A>
hash_b=<FNV64 of frame B>
ndiff=<number of differing pixels>
first=<x>,<y>
shift=h1|v1|none|other
```

- `h1`: every difference is explained by a one-pixel horizontal
  displacement (fine-X latch error class).
- `v1`: same for a one-scanline vertical displacement (coarse-Y/fine-Y
  latch error class).
- `none`: frames identical. `other`: anything else.

## Grading

The hidden manifest runs your tool over unseen frame pairs produced by a
reference renderer with seeded latch errors (one-pixel horizontal, one-
scanline vertical, plus an unrelated corruption). Your printed diagnosis
is checked verbatim. Rehearse with the public unit tests:
`ch22_99_frame_diff_tests`.
