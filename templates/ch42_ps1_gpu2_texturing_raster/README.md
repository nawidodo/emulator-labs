# ch42 — PS1 GPU II: Texturing & Rasterization Accuracy

Builds on GPU I: the same software-rasterizer architecture, now with
texture pages, CLUT lookup, blend modes, dithering, exact drawing-area
clipping and mask bits. Namespace `psx::gpu`; runners consume raw
little-endian u32 GP0 word streams (`--rom`) and write an FNV-1a-64 digest
of the full VRAM dump (`--hash-frame`).

## Exercise map

| Dir | Topic | Graded stages |
|---|---|---|
| `01_texture_pages` | page addressing, texture window, 4/8/15bpp fetch | `wrap_u/wrap_v`, `fetch_texel_4bpp`, `fetch_texel_8bpp`, `fetch_texel_15bpp` |
| `02_blend_clip` | modulate/decal, semi modes, transparency rules, dither, clip+offset, mask | `offset_coord`, `clip_in_draw_area`, `texel_transparent`, `mask_blocks`, `modulate_rgb`, `dither_apply`, `semi_blend` |
| `90_debug` | two seeded bugs (odd-page lane mirror missing; exclusive bottom/right clip) | `debug.regression_*` tests RED on stub, GREEN on fix |
| `91_challenge` | textured quad via two `24h` triangles + `74h/75h` rects to a golden VRAM hash | acceptance = embedded + fixture hash |
| `99_coding_test` | unseen textured primitive; hidden fixture graded by exact VRAM hash | `in_draw_area`, `texture_fetch`, `blend_pixel` |

Shared infrastructure lives in `shared/` (no graded code): VRAM model,
register-field decoding, colour helpers, dither table, the GP0 stream parser,
the rasterizer skeleton, and the runner CLI. Every exercise's solution keeps
the four pipeline stages visibly separate (`primitive_setup` → rasterize →
`texture_fetch` → `blend_pixel`); see each exercise's SPEC.md.

## Fixtures & goldens

Public fixtures and goldens: `tests/public/ch42_ps1_gpu2_texturing_raster/`
(see provenance.md there). The hidden manifest:
`tests/hidden/ch42_ps1_gpu2_texturing_raster/manifest.json` — its cases run
against solution-tree binaries (`build/solutions/...`), so grade with
`LABS_BUILD_SOLUTIONS=On` (or `make solutions`).

## Gate checklist

- [x] Skeletons build; unit suites RED on stubs (01, 02, 90_debug)
- [x] Solutions ALL GREEN (unit tests + challenge golden hash)
- [x] 90_debug DEBUGGING.md symptoms documented; both regressions isolate
- [x] 91_challenge golden reproduced twice from committed fixture
- [x] 99_coding_test hidden fixture + manifest golden validated
- [ ] Student: bug-report.md for 90_debug

## Verification

Recorded runs against this tree:

```
VERIFY_PREFIX=/tmp/labs-ch42gpu tools/labs/verify_chapter.sh ch42_ps1_gpu2_texturing_raster
# [verify] SKEL: build OK; ctest: 67% tests passed, 3 failed out of 9 (red failures expected)
#   failing skel suites = ch42_01_texture_pages, ch42_02_blend_clip, ch42_90_debug (graded blocks)
# [verify] SOLUTIONS: GREEN — 100% tests passed out of 9
# [verify] verdict: skel_build=ok solutions=GREEN
```

Hidden-manifest case validation (solution-tree binaries, hashes recomputed
with FNV-1a-64 over the produced files) — see the chapter report notes;
all real cases pass, the `requires_rom` ps1-tests case is skipped honestly.
