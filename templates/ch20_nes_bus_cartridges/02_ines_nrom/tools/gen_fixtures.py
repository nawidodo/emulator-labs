#!/usr/bin/env python3
"""Generate the synthetic NROM test ROMs for ch20.

Outputs (committed under tests/public/ch20_nes_bus_cartridges/roms/):
  boot_success.nes   16KB PRG / 8KB CHR, horizontal mirroring. Reset code
                     writes the success code $77 to RAM $02 and loops.
  vertical.nes       same image, flag6 bit0 = 1 (vertical arrangement).
  chrram.nes         CHR bank count 0: cartridge with 8KB CHR-RAM.

All bytes here are course-original; nothing is extracted from a
commercial ROM.
"""
import pathlib

OUT = pathlib.Path(__file__).resolve().parents[4] / "tests/public/ch20_nes_bus_cartridges/roms"


def ines(prg: bytes, chr_rom: bytes, f6: int) -> bytes:
    assert len(prg) % 16384 == 0
    header = bytearray(16)
    header[0:4] = b"NES\x1a"
    header[4] = len(prg) // 16384
    header[5] = len(chr_rom) // 8192
    header[6] = f6 & 0x0F            # mapper 0, flags in low nibble
    return bytes(header) + prg + chr_rom


# Boot program at $8000: init stack, write success code to $02, loop.
CODE = bytes([
    0x78,                   # 8000 SEI
    0xD8,                   # 8001 CLD
    0xA2, 0xFF,             # 8002 LDX #$FF
    0x9A,                   # 8004 TXS
    0xA9, 0x77,             # 8005 LDA #$77
    0x85, 0x02,             # 8007 STA $02      <- the success code
    0xA9, 0x55,             # 8009 LDA #$55     <- second witness cell
    0x85, 0x03,             # 800B STA $03
    0xE6, 0x04,             # 800D INC $04      <- heartbeat counter (RMW)
    0x4C, 0x0D, 0x80,       # 800F JMP $800D    # loop on INC
])
prg = bytearray(CODE)
prg += bytes(16384 - len(prg) - 6)
# vectors (NMI/RESET/IRQ at end of the 16KB bank)
prg += bytes([0x00, 0x80,        # FFFA NMI -> $8000
              0x00, 0x80,        # FFFC RESET -> $8000
              0x00, 0x80])       # FFFE IRQ -> $8000

chr_rom = bytes(8192)

OUT.mkdir(parents=True, exist_ok=True)
(OUT / "boot_success.nes").write_bytes(ines(bytes(prg), chr_rom, f6=0x00))
(OUT / "vertical.nes").write_bytes(ines(bytes(prg), chr_rom, f6=0x01))

# CHR-RAM variant: no CHR banks; emulators allocate 8KB writable CHR.
(OUT / "chrram.nes").write_bytes(ines(bytes(prg), b"", f6=0x00))

print("wrote ROMs to", OUT)
for p in sorted(OUT.glob("*.nes")):
    print(" ", p.name, p.stat().st_size, "bytes")
