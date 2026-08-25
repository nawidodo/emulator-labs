#!/usr/bin/env python3
"""Deterministic NESF v1 scene generator for ch21_nes_ppu1.

Emits the crafted PPU-state snapshots referenced by the chapter challenge
and hidden manifests. Run from anywhere:

    python3 gen_scene.py <output-dir>

Scenes:
  scene_grid.nesf   checkerboard of two tiles, four bg palettes (vertical)
  scene_attr.nesf   attribute quadrant wall, one palette per quadrant block
  scene_mirror.nesf art in physical page 1, visible only through
                    horizontal mirroring + PPUCTRL NT select ($2800)
"""
import sys
import pathlib

MAGIC = b"NESF"


def nesf(mirroring, ctrl, mask, oam_addr, fine_x, v, t, chr_, nt, pal, oam):
    assert len(chr_) == 0x2000 and len(nt) == 0x800 and len(pal) == 0x20
    assert len(oam) == 0x100
    out = bytearray()
    out += MAGIC
    out += bytes([1, mirroring, ctrl, mask, oam_addr, fine_x])
    out += v.to_bytes(2, "little") + t.to_bytes(2, "little")
    out += bytes(chr_) + bytes(nt) + bytes(pal) + bytes(oam)
    assert len(out) == 10542
    return bytes(out)


def tile_solid(chr_, idx, color):
    lo = 0xFF if color & 1 else 0x00
    hi = 0xFF if color & 2 else 0x00
    for y in range(16):
        chr_[idx * 16 + y] = hi if y >= 8 else lo


def scene_grid():
    chr_ = bytearray(0x2000)
    nt = bytearray(0x800)
    pal = bytearray(0x20)
    oam = bytearray(0x100)
    tile_solid(chr_, 1, 1)          # color 1 checker A
    tile_solid(chr_, 2, 2)          # color 2 checker B
    for r in range(30):
        for c in range(32):
            nt[r * 32 + c] = 1 if (r + c) % 2 == 0 else 2
    # One distinct palette per attribute quadrant-block column pair.
    for i in range(64):
        nt[0x3C0 + i] = (i % 4) * 0b01010101 & 0xFF
    for p in range(4):
        pal[p * 4 + 1] = 0x11 + p * 4
        pal[p * 4 + 2] = 0x21 + p * 4
    pal[0x00] = 0x0F
    return nesf(1, 0x00, 0x08, 0, 0, 0, 0, chr_, nt, pal, oam)  # vertical


def scene_attr():
    chr_ = bytearray(0x2000)
    nt = bytearray(0x800)
    pal = bytearray(0x20)
    oam = bytearray(0x100)
    tile_solid(chr_, 3, 3)
    for r in range(30):
        for c in range(32):
            nt[r * 32 + c] = 3
    for br in range(8):
        for bc in range(8):
            nt[0x3C0 + br * 8 + bc] = ((br + bc) % 4)
    for p in range(4):
        pal[p * 4 + 3] = [0x16, 0x22, 0x2A, 0x30][p]
    pal[0x00] = 0x0F
    return nesf(0, 0x00, 0x08, 0, 0, 0, 0, chr_, nt, pal, oam)  # horizontal


def scene_mirror():
    chr_ = bytearray(0x2000)
    nt = bytearray(0x800)
    pal = bytearray(0x20)
    oam = bytearray(0x100)
    tile_solid(chr_, 1, 1)
    tile_solid(chr_, 2, 3)
    # Art ONLY in physical page 1 ($2800 window under vertical wiring).
    for r in range(30):
        for c in range(32):
            nt[0x400 + r * 32 + c] = 1 if (c // 4) % 2 == 0 else 2
    pal[0x01] = 0x12
    pal[0x03] = 0x27
    pal[0x00] = 0x0F
    # horizontal mirroring: $2800 shares physical page 1 -> visible when
    # PPUCTRL selects nametable 2.
    return nesf(0, 0x02, 0x08, 0, 0, 0, 0x2800, chr_, nt, pal, oam)


def main():
    outdir = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".")
    outdir.mkdir(parents=True, exist_ok=True)
    for name, blob in [("scene_grid.nesf", scene_grid()),
                       ("scene_attr.nesf", scene_attr()),
                       ("scene_mirror.nesf", scene_mirror())]:
        (outdir / name).write_bytes(blob)
        print(f"wrote {outdir / name} ({len(blob)} bytes)")


if __name__ == "__main__":
    main()
