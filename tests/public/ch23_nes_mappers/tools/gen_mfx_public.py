#!/usr/bin/env python3
"""Generate the PUBLIC MFX-1 rehearsal cart + op script for ch23's coding
test (course-original, deterministic pattern bytes)."""
import os
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else "fixtures"
os.makedirs(OUT, exist_ok=True)

b = bytearray(b"NES\x1a")
b += bytes([4, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])   # 64 KiB PRG, 8 KiB CHR
b[6] |= (0x99 & 0x0F) << 4
b[7] |= 0x99 & 0xF0
for bank in range(4):
    b += bytes([(0xB0 + bank)] * 16384)
for unit in range(4):                                # 4 x 2 KiB CHR units
    b += bytes([(0x30 + unit)] * 2048)
open(os.path.join(OUT, "mfx_echo.nes"), "wb").write(bytes(b))

script = """# ch23 coding test rehearsal — MFX-1 probe over mfx_echo.nes
wr 8000 02          # R0 = PRG bank 2
rd 8000             # -> b2
rd bfff             # -> b2
rd c000             # mode A: last bank (b3)
wr 8001 01          # R1 bit0: mirror R0 into the high half
rd c000             # -> b2 now
rd ffff             # -> b2
wr 8002 01          # R2 = CHR unit 1: echo window
prd 0000            # unit 1 fill
prd 07ff            # still unit 1
prd 0800            # echo quadrant: unit 1 again
prd 17ff            # ...and the far corner too
wr 6abc 42          # PRG RAM
rd 6abc
snap                # r0=02 r1=01 r2=01 wc=7 irq=0
"""
open(os.path.join(OUT, "mfx_probe.txt"), "w").write(script)
print("public MFX fixtures written to", OUT)
