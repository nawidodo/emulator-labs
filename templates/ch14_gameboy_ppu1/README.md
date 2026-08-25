# ch14_gameboy_ppu1 — PPU I: tiles, tilemaps, background, window

Builds the content half of the DMG PPU as pure, headless functions over
a VRAM+register *snapshot image* (no CPU needed yet):

```text
01_tile_decode    2bpp two-plane interleaved tiles
02_bgp_palette    BGP translation + fixed grayscale ramp
03_bg_scanline    32x32 map fetch, scroll wrap, $8000/$8800 addressing
04_full_frame     160x144 RGBA frame + headless runner CLI
05_window         window layer with its internal line counter
90_debug          three seeded BG-render defects (bug-report required)
91_challenge      render committed scenes; match golden frame hashes
```

## Gate checklist

- [ ] exercises: all suites green (`LABS=ch14_gameboy_ppu1 make skels && make test`)
- [ ] starter: `ch14_04_frame_runner --help` works; smoke snapshot renders
- [ ] debug: 90_debug fixed AND `bug-report.md` written
- [ ] challenge: three golden hashes match (see 91_challenge/CHALLENGE.md)
- [ ] coding_test: `make grade GRADE_TARGETS=ch14_gameboy_ppu1` exits 0

## Snapshot image format (v1)

8200 bytes: `[0..0x1FFF]` VRAM, then LCDC, BGP, SCY, SCX, WY, WX.
Committed snapshots live in `tests/public/ch14_gameboy_ppu1/snapshots/`
with a generator and provenance note next to them.

## Runner CLI

Mandatory shape (`--rom --headless --cycles --frames --trace
--hash-frame --input-file`). For this chapter `--rom` loads the snapshot
image; `--hash-frame PATH` writes the raw RGBA8 dump to hash with
`tools/labs/hash_frame.py`. No chapter-specific extensions.

## Verification

Recorded after authoring; see the Verification section at the bottom of
this file once the chapter ships.

## Verification (recorded)

```text
VERIFY_PREFIX=/tmp/labs-GB2 tools/labs/verify_chapter.sh ch14_gameboy_ppu1
  SKEL:      build OK; ctest RED (6/7 failing as expected)
  SOLUTIONS: GREEN — 100% tests passed out of 7
grade.py (solution binaries vs tests/hidden/ch14_gameboy_ppu1):
  3/3 hidden cases PASS (+1 optional requires_rom skip, mooneye absent)
Goldens generated twice from reference runner, byte-identical:
  plain_tiles FE84FA8188FCF325 / scrolled_signed DBF293A93EFDA745 /
  window_scene 29CE20184355B925 (see tests/public/ch14_gameboy_ppu1/)
