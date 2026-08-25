#!/usr/bin/env python3
"""Generate ch14 PPU snapshot fixtures deterministically (no RNG, no time).

Outputs (next to this script's parent):
  fixtures/tile_arrow.bin          16-byte tile used by exercise 01
  snapshots/plain_tiles.ppu        unscrolled identity-BGP scene
  snapshots/scrolled_signed.ppu    scroll wrap + $8800 signed tiles
  snapshots/window_scene.ppu       window overlay scene

Each snapshot is the v1 PPU state image: 8 KB VRAM followed by
LCDC, BGP, SCY, SCX, WY, WX. Run from anywhere:
  python3 tests/public/ch14_gameboy_ppu1/tools/make_snapshots.py <repo-root>
"""
import sys
from pathlib import Path


def tile_bytes(rows):
    """rows: list of 8 lists of 8 color indices -> 16 interleaved bytes."""
    out = bytearray()
    for y in range(8):
        lo = 0
        hi = 0
        for x in range(8):
            idx = rows[y][x] & 3
            lo |= ((idx & 1) << (7 - x))
            hi |= (((idx >> 1) & 1) << (7 - x))
        out += bytes([lo, hi])
    return bytes(out)


def pack(vram, lcdc, bgp, scy, scx, wy, wx):
    return bytes(vram) + bytes([lcdc, bgp, scy, scx, wy, wx])


def main(repo_root):
    pub = Path(repo_root) / "tests" / "public" / "ch14_gameboy_ppu1"
    fix = pub / "fixtures"
    snap = pub / "snapshots"
    fix.mkdir(parents=True, exist_ok=True)
    snap.mkdir(parents=True, exist_ok=True)

    # --- fixture tile (matches templates/ch14.../01_tile_decode/main.cpp)
    # Byte-exact twin of kTile in templates/ch14_gameboy_ppu1/
    # 01_tile_decode/main.cpp: rising diagonal (index 2 with an index-1
    # left neighbor), bottom bar mostly index 3.
    arrow = []
    for y in range(6):
        row = [0] * 8
        row[6 - y] = 1
        row[7 - y] = 2
        arrow.append(row)
    arrow.append([0, 1, 3, 3, 3, 3, 3, 2])
    arrow.append([0] * 8)
    (fix / "tile_arrow.bin").write_bytes(tile_bytes(arrow))

    t_half = tile_bytes(
        [[1 if x < 4 else 2 for x in range(8)] for _ in range(8)])
    t_checker = tile_bytes(
        [[(1 if (x // 2 + y // 2) % 2 == 0 else 2) for x in range(8)]
         for y in range(8)])
    vram = bytearray(0x2000)
    vram[0x10:0x20] = t_half
    vram[0x20:0x30] = t_checker
    for row in range(32):
        for col in range(32):
            vram[0x1800 + row * 32 + col] = 1 if row % 2 == col % 2 else 2
    (snap / "plain_tiles.ppu").write_bytes(pack(vram, 0x91, 0xE4,
                                                0, 0, 0, 0))

    # --- scrolled_signed.ppu ---------------------------------------------
    # $8800 signed addressing: map byte 0x00 -> tile at $1000 (tile 0),
    # 0x01 -> +1, 0xFF -> -1 ($0FF0), 0x81 -> +1... we use 0x80 (=0).
    t_diag = tile_bytes(
        [[3 if x == y else 0 for x in range(8)] for y in range(8)])
    t_cross = tile_bytes(
        [[3 if (x in (3, 4)) or (y in (3, 4)) else 1 for x in range(8)]
         for y in range(8)])
    vram = bytearray(0x2000)
    vram[0x0010:0x0020] = t_diag      # signed -127 ... also unsigned 1
    vram[0x0FE0:0x0FF0] = t_cross     # signed -2 ($0FE0)
    vram[0x1020:0x1030] = t_checker   # signed +2 ($1020)
    for row in range(32):
        for col in range(32):
            # mix negative, zero and positive signed tile numbers
            vram[0x1800 + row * 32 + col] = (
                [0xFE, 0x00, 0x02, 0xFF][(row + col) % 4])
    (snap / "scrolled_signed.ppu").write_bytes(pack(vram, 0x81, 0x1B,
                                                    57, 103, 0, 0))

    # --- window_scene.ppu -------------------------------------------------
    t_stripe_a = tile_bytes([[2] * 8 for _ in range(8)])
    t_stripe_b = tile_bytes([[0] * 8 for _ in range(8)])
    vram = bytearray(0x2000)
    vram[0x10:0x20] = t_half
    vram[0x20:0x30] = t_stripe_a
    vram[0x30:0x40] = t_stripe_b
    for i in range(0x800):
        vram[0x1800 + i] = 1
    for row in range(32):
        for col in range(32):
            vram[0x1C00 + row * 32 + col] = 2 if col % 2 == 0 else 3
    (snap / "window_scene.ppu").write_bytes(pack(vram, 0xF1, 0xE4,
                                                 0, 0, 32, 7 + 40))

    print("wrote", fix / "tile_arrow.bin")
    for name in ("plain_tiles.ppu", "scrolled_signed.ppu",
                 "window_scene.ppu"):
        print("wrote", snap / name)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else ".")
