# Chapter 41 — PS1 GPU I: Command FIFO, VRAM and Basic Rasterization

The PlayStation GPU owns 1 MiB of VRAM (1024x512, 15-bit BGR555) reachable
only through two ports. This chapter builds the packet layer (GP0/GP1), the
three block transfers with their exact PSX-SPX masking quirks, the FILL
command, GPUSTAT, and a deterministic integer rasterizer for untextured
triangles/quads/rectangles. Texturing, blending, dithering and the CLUT
pipeline are ch42.

Primary reference: PSX-SPX <https://problemkaputt.de/psx-spx.htm>.
`LECTURE.md` is the full lecture; every formula is quoted or derived there.

## Exercise map

| dir | content |
|---|---|
| `01_vram` | VRAM model + GP0(A0h/C0h/80h): independent X/Y wrap, size-0-means-max, overlap-safe copies |
| `02_gp0_gp1` | command FIFO, packet state machine, GP1 display control, GPUSTAT bit-exact composition, FILL quirks |
| `03_rect_tri` (**starter**) | software rasterizer: top-left fill convention, backface culling by signed area, flat/Gouraud triangles in Q12, rectangles |
| `91_challenge` | complete reference GPU + runner; render a GP0 stream to a golden VRAM hash |
| `99_coding_test` | same pipeline minus the triangle core — implement it to hit a hidden golden exactly |

There is no `90_debug` this chapter.

## Runner CLI

```
ch41_91_challenge_runner --rom FILE [--frames N] [--cycles N] [--headless]
                         [--trace FILE] [--hash-frame FILE]
```

* `--rom`: raw little-endian u32 word stream executed as GP0 writes.
  GP1 commands cannot be distinguished from GP0 packets on a raw wire
  (both share the opcode space), so the harness reserves undocumented GP0
  opcode `12h` as an escape: the word `12000000h` means "the next word is a
  raw GP1 command". Fixture data avoids that one value.
* `--frames N`: execute the stream N times. Every shipped fixture begins
  with a register-neutralization prologue and is idempotent under
  repetition.
* `--trace FILE`: one line per consumed word,
  `pc=<byte-offset hex> op=<word hex> cyc=<n>`.
* `--hash-frame FILE`: writes `fnv64=<16 uppercase hex>\n` — FNV-1a-64 over
  the full little-endian 1024x512x2-byte VRAM image (same algorithm as
  `tools/labs/grade.py`).
* Unknown flags exit nonzero with usage on stderr; `--help` exits 0.

## Normative rasterization summary

Full details in `03_rect_tri/SPEC.md`; the short form: pixel-centre sample
rule on a doubled lattice, top-left fill convention (no double-drawn shared
edges), cull `signed_area <= 0`, Gouraud weights `(E<<12)/(4*area2)` with
round-half-up channel resolve then `>>3` to 5 bits, quads split per PSX-SPX
into (1,2,3)+(2,3,4). Dither disabled chapter-wide.

## Gate checklist

- [ ] `make skels && make test`: skeletons build, exercise tests RED
- [ ] solution tree: all ch41 tests GREEN
- [ ] `01_vram`: five TODO blocks (index/wrap, sizes, three transfers)
- [ ] `02_gp0_gp1`: seven TODO blocks (param table, FIFO, GP1, parser, exec, GPUREAD, GPUSTAT)
- [ ] `03_rect_tri`: starter implemented per SPEC
- [ ] `91_challenge`: acceptance criteria in `CHALLENGE.md` met
- [ ] `99_coding_test`: hidden golden matched (`coding_test_unseen_triangle`)

## Verification

Recorded from the authoring pass (isolated prefix):

```
VERIFY_PREFIX=/tmp/labs-PS1A tools/labs/verify_chapter.sh ch41_ps1_gpu1_commands_vram
  -> skeleton build OK (tests RED as expected: 01/02/03 suites)
  -> SOLUTIONS: GREEN — 100% tests passed out of 5
```

Hidden manifest validated by direct execution of solution-tree binaries,
each hash case run twice and confirmed deterministic:

| case | result |
|------|--------|
| unit_tests_vram_transfers | exit 0 |
| transfer_roundtrip_wrap | exit 0, fnv64 matches `6EE6E5DADDE8E7DB` |
| fill_param_quirks | exit 0, fnv64 matches `E8B19D579D9B7409` |
| gouraud_and_mask_golden | exit 0, fnv64 matches `1A336C10AD4084B2` |
| coding_test_unseen_triangle | exit 0, fnv64 matches `107E362E986885AE` |
| ps1_tests_gpu_suite_optional | skipped (requires_rom absent) |
