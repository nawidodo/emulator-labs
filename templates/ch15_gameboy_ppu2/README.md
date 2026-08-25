# ch15_gameboy_ppu2 — PPU II: sprites, modes, STAT, full pipeline

Builds the timing half of the DMG PPU on top of chapter 14's content
half, still as pure headless functions over a snapshot image (v2 — now
with OAM, sprite palettes and LYC):

```text
01_oam_sprites    OAM scan: 10-per-line limit, priorities, flips, 8x16
02_mode_timing    dot-driven mode machine (2/3/0/1), VRAM+OAM locks,
                  deterministic mode-transition trace
03_stat_irq       STAT sources, LYC coincidence, rising-edge IRQ quirk
04_full_ppu       complete renderer + ch15_04_fullppu_runner CLI
90_debug          five seeded raster defects (bug-report required)
91_challenge      three crafted scenes vs golden frame hashes
99_coding_test    repair-from-snapshot methodology (docs)
```

## Gate checklist

- [ ] exercises: all suites green (`LABS=ch15_gameboy_ppu2 make skels && make test`)
- [ ] starter: `ch15_04_fullppu_runner --help` works; smoke snapshot renders
- [ ] debug: 90_debug fixed AND `bug-report.md` written
- [ ] challenge: three golden hashes match (see 91_challenge/CHALLENGE.md)
- [ ] coding_test: `make grade GRADE_TARGETS=ch15_gameboy_ppu2` exits 0

## Snapshot image format v2 ("PPU state file v2", little-endian)

```text
offset  size    content
0x0000  0x2000  VRAM ($8000-$9FFF)
0x2000  0x00A0  OAM   ($FE00-$FE9F; 40 entries x 4 bytes: y, x, tile, flags)
0x20A0          LCDC   0x20A1 STAT  0x20A2 BGP   0x20A3 OBP0  0x20A4 OBP1
0x20A5  SCY     0x20A6 SCX    0x20A7 WY    0x20A8 WX    0x20A9 LYC
```

Total 8362 bytes (`0x20AA`). OAM flags: 0x80 BG-over-sprite priority,
0x40 Y flip, 0x20 X flip, 0x10 palette. Sprite Y/X carry a 16/8-pixel
offset; `x == 0` hides a sprite entirely and such entries are skipped
BEFORE the 10-per-line limit (documented course choice). LCDC bits:
80 LCD on, 40 win map hi, 20 win enable, 10 $8000 tiles, 08 bg map hi,
04 sprite size 8x16, 02 sprites enable, 01 bg enable.

Committed snapshots live in `tests/public/ch15_gameboy_ppu2/snapshots/`
with a generator and provenance note next to them.

## Runner CLI

Mandatory shape (`--rom --headless --cycles --frames --trace
--hash-frame --input-file`). For this chapter `--rom` loads the v2
snapshot image; every rendered frame is identical so `--frames` only
repeats work. Chapter extensions:

- `--hash-frame PATH` writes the raw RGBA8 dump to hash with
  `tools/labs/hash_frame.py`.
- `--trace FILE` writes the mode-transition log of the first frame
  (`ly=<n> dot=<n> mode=<m>\n` per change; same format as exercise 02's
  `buildModeTrace`, golden copy under `tests/public/.../traces/`).
- `--window-off-lines A:B` disables the window for screen lines [A,B)
  while rendering — models a mid-frame toggle of LCDC bit 5; skipped
  lines do NOT advance the window's internal content counter.

## Verification

Recorded after authoring; see the Verification section at the bottom of
this file once the chapter ships.

## Verification (recorded)

```text
VERIFY_PREFIX=/tmp/labs-Ch15 tools/labs/verify_chapter.sh ch15_gameboy_ppu2
  SKEL:      build OK; ctest RED (5/6 failing as expected; the failing
             one is only the runner --help smoke test passing)
  SOLUTIONS: GREEN — 100% tests passed out of 6
grade.py (solution binaries vs tests/hidden/ch15_gameboy_ppu2):
  4/4 hidden cases PASS (+1 optional requires_rom skip, mooneye absent)
Goldens generated twice from reference runner, byte-identical (cmp):
  sprites_limit     77962EF9AF6E57BD   combo_window(toggle 32:96) 844AC316A5B8DB25
  sprites_tall_flip 3EF685E7966603ED   combo_signed               E683CCF9204BB1A5
  combo_scroll      E08E3358DA7F5325   mode_trace.txt(442 lines)  84B8128AF36A346C
  combo_window      E4E0D388557CFB25
(see tests/public/ch15_gameboy_ppu2/goldens/goldens.md)
```
