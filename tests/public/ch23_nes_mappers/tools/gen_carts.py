#!/usr/bin/env python3
"""Generate synthetic iNES cartridges for ch23_nes_mappers (course-original,
no commercial ROM content: every byte is a deterministic pattern)."""
import os
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else "fixtures"

def header(mapper, prg_banks, chr_banks, trainer=False, four_screen=False,
           vertical=False):
    b = bytearray(b"NES\x1a")
    b += bytes([prg_banks, chr_banks, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
    if vertical:
        b[6] |= 0x01
    if trainer:
        b[6] |= 0x04
    if four_screen:
        b[6] |= 0x08
    b[6] |= (mapper & 0x0F) << 4
    b[7] |= mapper & 0xF0
    return b


def cart(path, mapper, prg_banks, chr_banks, fill_prg=None, fill_chr=None,
         **flags):
    blob = bytearray(header(mapper, prg_banks, chr_banks, **flags))
    if flags.get("trainer"):
        blob += bytes([0xEE]) * 512
    for bank in range(prg_banks):
        blob += bytes([fill_prg(bank, k) for k in range(16384)]) \
            if fill_prg else bytes([(0xB0 + bank) % 256]) * 16384
    for unit in range(chr_banks):
        blob += bytes([fill_chr(unit, k) for k in range(8192)]) \
            if fill_chr else bytes([(0x40 + unit) % 256]) * 8192
    with open(os.path.join(OUT, path), "wb") as f:
        f.write(blob)
    return path


cart("ux_4bank.nes", 2, 4, 0)
cart("cnrom_16kchr.nes", 3, 1, 2)
cart("mmc1_basic.nes", 1, 4, 8)
cart("mmc3_scanline.nes", 4, 8, 16,
     fill_chr=lambda u, k: ((u * 7 + k * 3) & 0x3F))
cart("trainer_mmc1.nes", 1, 2, 1, trainer=True)
cart("fourscreen_cnrom.nes", 3, 2, 1, four_screen=True)
print("carts written to", OUT)
