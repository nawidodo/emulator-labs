#!/usr/bin/env python3
"""Generate ch15 PPU v2 snapshot fixtures deterministically (no RNG, no time).

Public outputs (under tests/public/ch15_gameboy_ppu2/):
  snapshots/sprites_limit.ppu2      >10 overlapping sprites on one line,
                                    x==0 trap entries, limit vs priority
  snapshots/sprites_tall_flip.ppu2  8x16 mode, X/Y flips, OBP1, BG priority
  snapshots/combo_scroll.ppu2       scrolled checker BG + window + sprites
  snapshots/combo_window.ppu2       window at top + sprites (toggle target)
  snapshots/combo_signed.ppu2       $8800 signed tiles + window + sprites

Hidden outputs (under tests/hidden/ch15_gameboy_ppu2/fixtures/):
  h1_sprite_heavy.ppu2              dense sprite field, mixed flags
  h2_trace_scene.ppu2               scene used for the --trace hash case
  h3_window_toggle.ppu2             window scene for --window-off-lines

Snapshot image format "PPU state file v2" (little-endian):
  [0x0000..0x2000) VRAM
  [0x2000..0x20A0) OAM (40 entries x 4 bytes: y, x, tile, flags)
  0x20A0 LCDC 0x20A1 STAT 0x20A2 BGP 0x20A3 OBP0 0x20A4 OBP1
  0x20A5 SCY  0x20A6 SCX  0x20A7 WY   0x20A8 WX  0x20A9 LYC
Total 0x20AA = 8362 bytes. Run from anywhere:
  python3 tests/public/ch15_gameboy_ppu2/tools/make_snapshots.py <repo-root>
"""
import sys
from pathlib import Path


def tile_bytes(rows):
    """rows: 8 lists of 8 color indices -> 16 interleaved plane bytes."""
    out = bytearray()
    for y in range(8):
        lo = hi = 0
        for x in range(8):
            idx = rows[y][x] & 3
            lo |= (idx & 1) << (7 - x)
            hi |= ((idx >> 1) & 1) << (7 - x)
        out += bytes([lo, hi])
    return bytes(out)


def solid(idx):
    return tile_bytes([[idx] * 8 for _ in range(8)])


def even_stripes(idx):
    rows = [[idx if x % 2 == 0 else 0 for x in range(8)] for _ in range(8)]
    return tile_bytes(rows)


def diag(idx=3):
    return tile_bytes([[idx if x == y else 0 for x in range(8)]
                       for y in range(8)])


def corner():
    """Asymmetric tall-sprite pair: dark pixel top-left / bottom-right."""
    top = tile_bytes([[1 if (x == 0 and y == 0) else 0
                       for x in range(8)] for y in range(8)])
    bot = tile_bytes([[1 if (x == 7 and y == 7) else 0
                       for x in range(8)] for y in range(8)])
    return top + bot


def checker(a, b):
    return tile_bytes([[a if (x // 2 + y // 2) % 2 == 0 else b
                        for x in range(8)] for y in range(8)])


def pack(vram, oam, lcdc, stat, bgp, obp0, obp1, scy, scx, wy, wx, lyc):
    assert len(vram) == 0x2000 and len(oam) == 0xA0
    return bytes(vram) + bytes(oam) + bytes(
        [lcdc, stat, bgp, obp0, obp1, scy, scx, wy, wx, lyc])


def fresh_vram():
    return bytearray(0x2000)


def fresh_oam():
    return bytearray(0xA0)  # all zeros: y=0/x=0 -> inert hidden entries


def set_spr(oam, entry, y, x, tile, flags):
    oam[4 * entry:4 * entry + 4] = bytes([y, x, tile, flags])


def fill_map(vram, base, entry):
    vram[base:base + 0x800] = bytes([entry]) * 0x800


LCD = 0x80          # LCD on
T_UNSIGNED = 0x10   # $8000 tiles
SPR_EN = 0x02
BG_EN = 0x01
WIN_EN = 0x20
WIN_MAP_HI = 0x40
BG_MAP_HI = 0x08
SPR_16 = 0x04


def main(repo_root):
    pub = Path(repo_root) / "tests" / "public" / "ch15_gameboy_ppu2"
    hid = Path(repo_root) / "tests" / "hidden" / "ch15_gameboy_ppu2"
    (pub / "snapshots").mkdir(parents=True, exist_ok=True)
    (hid / "fixtures").mkdir(parents=True, exist_ok=True)

    # Shared tile vocabulary ($8000 unsigned addressing).
    T_SOLID0 = solid(0)
    T_CHECK12 = checker(1, 2)
    T_DIAG3 = diag(3)
    T_STRIPE3 = even_stripes(3)
    T_CORNER = corner()          # occupies TWO consecutive tiles

    def emit(name, dest_dir, vram, oam, lcdc, bgp=0xE4, obp0=0xE4,
             obp1=0x1B, scy=0, scx=0, wy=0, wx=0, lyc=100):
        p = dest_dir / name
        p.write_bytes(pack(vram, oam, lcdc, 0x00, bgp, obp0, obp1,
                           scy, scx, wy, wx, lyc))
        print("wrote", p)

    # --- sprites_limit.ppu2 ------------------------------------------------
    # 14 entries cover line 60; the first ten in OAM order win regardless
    # of their larger x. Entry 11 has a smaller x but must be ignored.
    # Entries 12-13 are x==0 traps that must not consume limit slots.
    vram = fresh_vram()
    vram[0x00:0x10] = T_SOLID0     # tile 1: white BG
    vram[0x20:0x30] = T_DIAG3      # tile 2: dark sprite pattern
    fill_map(vram, 0x1800, 1)
    oam = fresh_oam()
    for i in range(10):
        set_spr(oam, i, 70, 100 - 6 * i, 2, 0)
    set_spr(oam, 10, 70, 20, 2, 0)     # smaller x — still beyond the limit
    set_spr(oam, 11, 70, 150, 2, 0)    # beyond the limit as well
    set_spr(oam, 12, 70, 0, 2, 0)      # x==0: skipped entirely
    set_spr(oam, 13, 70, 0, 2, 0)      # x==0 again
    emit("sprites_limit.ppu2", pub / "snapshots", vram, oam,
         LCD | T_UNSIGNED | SPR_EN | BG_EN)

    # --- sprites_tall_flip.ppu2 --------------------------------------------
    # LCDC bit 2 set: every sprite is 8x16. Flips and palettes vary; the
    # last two entries sit over a nonzero BG with the priority flag.
    vram = fresh_vram()
    vram[0x00:0x10] = T_CHECK12    # tile 1: checker BG (nonzero everywhere)
    vram[0x400:0x420] = T_CORNER   # tiles 0x40/0x41 asymmetric pair
    fill_map(vram, 0x1800, 1)
    oam = fresh_oam()
    set_spr(oam, 0, 40, 40, 0x40, 0)                 # normal
    set_spr(oam, 1, 40, 60, 0x40, 0x20)              # X flip
    set_spr(oam, 2, 40, 80, 0x40, 0x40)              # Y flip
    set_spr(oam, 3, 40, 100, 0x40, 0x60)             # X+Y flip
    set_spr(oam, 4, 80, 40, 0x42, 0x10)              # OBP1 (tiles 0x42/43
    vram[0x420:0x440] = T_CORNER                     # share the pair bytes)
    set_spr(oam, 5, 80, 60, 0x44, 0x90)              # OBP1 + priority
    vram[0x440:0x460] = T_CORNER
    set_spr(oam, 6, 80, 80, 0x46, 0x80)              # priority only -> hidden
    vram[0x460:0x480] = T_CORNER
    emit("sprites_tall_flip.ppu2", pub / "snapshots", vram, oam,
         LCD | T_UNSIGNED | SPR_16 | SPR_EN | BG_EN)

    # --- combo_scroll.ppu2 ---------------------------------------------------
    vram = fresh_vram()
    vram[0x00:0x10] = T_CHECK12
    vram[0x10:0x20] = T_SOLID0
    vram[0x30:0x40] = T_DIAG3
    for r in range(32):
        for c in range(32):
            vram[0x1800 + r * 32 + c] = 1 if (r + c) % 2 else 2
    for i in range(0x400):
        vram[0x1C00 + i] = 3 if (i % 32) % 2 == 0 else 1  # window stripes
    vram[0x20:0x30] = T_SOLID0
    oam = fresh_oam()
    set_spr(oam, 0, 60, 48, 3, 0)      # dark sprite over checker
    set_spr(oam, 1, 60, 88, 3, 0x80)   # priority flag -> hidden here
    set_spr(oam, 2, 120, 24, 3, 0x10)  # OBP1 sprite near the bottom
    emit("combo_scroll.ppu2", pub / "snapshots", vram, oam,
         LCD | WIN_EN | WIN_MAP_HI | T_UNSIGNED | SPR_EN | BG_EN,
         bgp=0x1B, scy=57, scx=103, wy=96, wx=7)

    # --- combo_window.ppu2 ---------------------------------------------------
    # Window covers the top of the screen; --window-off-lines 32:96 must
    # skip exactly those content rows.
    vram = fresh_vram()
    vram[0x00:0x10] = T_SOLID0
    vram[0x10:0x20] = T_STRIPE3
    vram[0x20:0x30] = T_DIAG3
    fill_map(vram, 0x1800, 1)   # BG map: plain white surface
    for i in range(0x400):
        vram[0x1C00 + i] = 2 if (i % 32) % 2 == 0 else 1  # window stripes
    oam = fresh_oam()
    for i in range(4):
        set_spr(oam, i, 130, 30 + 20 * i, 3, 0)
    emit("combo_window.ppu2", pub / "snapshots", vram, oam,
         LCD | WIN_EN | WIN_MAP_HI | T_UNSIGNED | SPR_EN | BG_EN,
         wy=0, wx=7)

    # --- combo_signed.ppu2 ---------------------------------------------------
    # $8800 signed addressing on BOTH maps: byte $FF -> tile -1 ($0FE0),
    # $02 -> +2 ($1020), etc.
    vram = fresh_vram()
    vram[0x0010:0x0020] = diag(2)     # also unsigned tile 1
    vram[0x0FE0:0x0FF0] = checker(3, 0)   # signed -2
    vram[0x1020:0x1030] = solid(1)        # signed +2
    for r in range(32):
        for c in range(32):
            vram[0x1800 + r * 32 + c] = [0xFE, 0x00, 0x02, 0xFF][(r + c) % 4]
    for i in range(0x400):
        vram[0x1C00 + i] = [0xFF, 0x02][(i % 32) % 2]
    oam = fresh_oam()
    set_spr(oam, 0, 50, 60, 2, 0)
    set_spr(oam, 1, 50, 90, 2, 0x80)   # priority-hidden over checker
    set_spr(oam, 2, 110, 140, 2, 0x30) # OBP1 + flips
    emit("combo_signed.ppu2", pub / "snapshots", vram, oam,
         LCD | WIN_EN | WIN_MAP_HI | SPR_EN | BG_EN,
         bgp=0xE4, scy=0, scx=0, wy=72, wx=7)

    # --- hidden fixtures -----------------------------------------------------
    # h1: dense sprite field mixing everything that can go wrong in the
    # sprite pass (limit order, x==0 traps, priority, palettes, flips).
    vram = fresh_vram()
    vram[0x00:0x10] = T_CHECK12
    vram[0x10:0x20] = T_SOLID0
    vram[0x20:0x30] = T_DIAG3
    vram[0x30:0x40] = T_STRIPE3
    fill_map(vram, 0x1800, 1)
    oam = fresh_oam()
    e = 0
    for band, base_y in enumerate((40, 70, 100)):
        for k in range(5):
            if e >= 38:
                break
            tile = 2 if k % 2 == 0 else 3
            flags = 0
            if k == 1:
                flags |= 0x20           # X flip
            if k == 2:
                flags |= 0x10           # OBP1
            if k == 3:
                flags |= 0x80           # priority (checker BG hides it)
            if k == 4:
                set_spr(oam, e, base_y, 0, tile, 0)   # x==0 trap
                e += 1
                continue
            set_spr(oam, e, base_y, 12 + 18 * k, tile, flags)
            e += 1
    emit("h1_sprite_heavy.ppu2", hid / "fixtures", vram, oam,
         LCD | T_UNSIGNED | SPR_EN | BG_EN)

    # h2: scene rendered by the trace-hash case (timing-only output, but
    # keep a real scene so the case also exercises frame rendering).
    vram = fresh_vram()
    vram[0x00:0x10] = T_CHECK12
    vram[0x10:0x20] = T_SOLID0
    fill_map(vram, 0x1800, 1)
    for r in range(32):
        for c in range(32):
            vram[0x1800 + r * 32 + c] = 1 if (r // 8) % 2 else 2
    oam = fresh_oam()
    set_spr(oam, 0, 90, 80, 2, 0)
    emit("h2_trace_scene.ppu2", hid / "fixtures", vram, oam,
         LCD | WIN_EN | WIN_MAP_HI | T_UNSIGNED | SPR_EN | BG_EN,
         scy=33, scx=77, wy=112, wx=7, lyc=143)

    # h3: window-toggle scene (rendered with --window-off-lines 32:96).
    vram = fresh_vram()
    vram[0x00:0x10] = T_SOLID0
    vram[0x10:0x20] = T_STRIPE3
    vram[0x20:0x30] = T_DIAG3
    fill_map(vram, 0x1800, 1)
    for i in range(0x400):
        vram[0x1C00 + i] = 3 if (i % 64) < 32 else 2  # banded window rows
    oam = fresh_oam()
    set_spr(oam, 0, 140, 20, 3, 0)
    set_spr(oam, 1, 140, 160, 3, 0)
    emit("h3_window_toggle.ppu2", hid / "fixtures", vram, oam,
         LCD | WIN_EN | WIN_MAP_HI | T_UNSIGNED | SPR_EN | BG_EN,
         wy=8, wx=7, lyc=50)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else ".")
