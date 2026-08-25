# CODING_TEST — unseen-spec snapshot renderer

You get a specification you have never seen before: a line-based TEXT
snapshot format describing a tiny tilemap scene, plus the API contract to
construct and render it. `SPEC.md` is binding. Your job:

1. Implement the four `@LABS` blocks in `coding.hpp`
   (`parse_map_row`, `decode_tile_row`, `snapshot_sample`,
   `snapshot_render`).
2. Make the public suite pass:

```bash
./ch32_99_coding_tests
```

3. Render arbitrary conforming snapshots from the command line:

```bash
./ch32_99_coding_tests --snapshot scene.txt --hash-frame frame.rgba
```

Exit code is nonzero if any test fails or the rendered output disagrees
with the independent reference oracle baked into `main.cpp`. Grading runs
your binary against an UNSEEN snapshot fixture and checks both the exit
status and the FNV-64 of the rendered frame — so implement the spec, not
the example.

Rules: do not modify the parser framework section of `coding.hpp`; do not
special-case the example; no wall-clock, no RNG.
