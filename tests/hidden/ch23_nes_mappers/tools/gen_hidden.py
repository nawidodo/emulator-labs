#!/usr/bin/env python3
"""Generate the HIDDEN MFX-1 grading fixtures (unseen cart + unseen op
script). Course-original, deterministic pattern bytes."""
import os
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else "fixtures"
os.makedirs(OUT, exist_ok=True)

# MFX-1 unseen cart: 8 PRG banks, NO CHR ROM (MFX-1 falls back to flat
# CHR RAM), vertical-mirroring flag set (present in header, unused by the
# board).
b = bytearray(b"NES\x1a")
# Unseen iNES header fixture: mapper 0x63 needs BOTH nibble sources,
# trainer present, vertical mirroring.
h = bytearray(b"NES\x1a")
h += bytes([3, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
h[6] |= 0x01          # vertical
h[6] |= 0x04          # trainer
h[6] |= (0x03 & 0x0F) << 4
h[7] |= 0x60          # flags7 nibble -> mapper 0x63 (both sources needed)
blob = bytes(h) + bytes([0xEE]) * 512
for bank in range(3):
    blob += bytes([0xA0 + bank]) * 16384
for unit in range(2):
    blob += bytes([0x50 + unit]) * 8192
open(os.path.join(OUT, "unseen_header.nes"), "wb").write(blob)
b += bytes([8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
b[6] |= (0x99 & 0x0F) << 4
b[6] |= 0x01
b[7] |= 0x99 & 0xF0
for bank in range(8):
    b += bytes([(0x90 + bank * 0x11) & 0xFF] * 16384)
open(os.path.join(OUT, "unseen_mfx.nes"), "wb").write(bytes(b))

script = """# ch23 hidden coding test — unseen MFX-1 cart + script
wr 8000 05          # R0 = bank 5 of 8 (fill byte AF)
rd 8000
rd bfff
rd c000             # mode A: fixed LAST bank (bank 7, fill byte D1)
wr 8001 01          # R1 bit0: mirror mode B
rd c000             # now mirrors R0's bank
rd ffff
prd 1234            # CHR-RAM board: flat window reads zero unwritten
wr 6000 aa
rd 6000             # PRG RAM round-trip
snap
wr 8003 02          # arm one-shot timer, period 2 (register write #3)
wr 8015 00          # write #4 -> tick (count 2->1)
rd f000             # last-bank window still readable: bank 7 fill byte
wr 8102 07          # writes #5-#7: no tick
wr 8101 06
wr 8100 05
snap                # armed, count 1, quiet
wr 9000 04          # write #8 -> tick -> fires, latched
snap
wr 8003 01          # fresh R3 write: acknowledges AND rearms
snap
"""
open(os.path.join(OUT, "unseen_mfx_script.txt"), "w").write(script)
print("hidden MFX fixtures written to", OUT)
