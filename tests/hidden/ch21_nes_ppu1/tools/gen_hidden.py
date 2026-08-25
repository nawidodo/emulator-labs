#!/usr/bin/env python3
"""Hidden NESF fixture for the ch21 coding test (unseen nametable snapshot).

Distinct from every public scene: four-screen-style attribute spread on
vertical mirroring, tiles drawn from BOTH CHR halves via PPUCTRL bit 4.

    python3 gen_hidden.py <output-dir>   # writes unseen_vram.nesf
"""
import pathlib
import sys


def nesf(mirroring, ctrl, mask, oam_addr, fine_x, v, t, chr_, nt, pal, oam):
    out = bytearray()
    out += b"NESF"
    out += bytes([1, mirroring, ctrl, mask, oam_addr, fine_x])
    out += v.to_bytes(2, "little") + t.to_bytes(2, "little")
    out += bytes(chr_) + bytes(nt) + bytes(pal) + bytes(oam)
    assert len(out) == 10542
    return bytes(out)


def main():
    outdir = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".")
    outdir.mkdir(parents=True, exist_ok=True)

    chr_ = bytearray(0x2000)
    # Tile 1 (left table): diagonal stripes color 1/2.
    for y in range(8):
        for x in range(8):
            bit = 1 << (7 - x)
            c1 = (x + y) % 2 == 0
            if c1:
                chr_[16 + y] |= bit
            else:
                chr_[24 + y] |= bit
    # Same index in the RIGHT table: inverted diagonals.
    for y in range(8):
        for x in range(8):
            bit = 1 << (7 - x)
            if not ((x + y) % 2 == 0):
                chr_[0x1000 + 16 + y] |= bit
            else:
                chr_[0x1000 + 24 + y] |= bit

    nt = bytearray(0x800)
    for r in range(30):
        for c in range(32):
            nt[r * 32 + c] = 1
    # Attribute mosaic: palette index = (block_col % 2) | (block_row % 2) << 1
    for br in range(8):
        for bc in range(8):
            nt[0x3C0 + br * 8 + bc] = (bc % 2) | ((br % 2) << 1)

    pal = bytearray(0x20)
    pal[0x00] = 0x0F
    pal[0x01] = 0x21
    pal[0x02] = 0x26
    pal[0x05] = 0x2A
    pal[0x06] = 0x16
    pal[0x09] = 0x28
    pal[0x0A] = 0x18
    pal[0x0D] = 0x15

    blob = nesf(1, 0x10, 0x08, 0, 3, 0x2000, 0x23C0, chr_, nt, pal,
                bytearray(0x100))
    (outdir / "unseen_vram.nesf").write_bytes(blob)
    print(f"wrote {outdir / 'unseen_vram.nesf'} ({len(blob)} bytes)")


if __name__ == "__main__":
    main()
