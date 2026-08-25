#!/usr/bin/env python3
"""Hidden fixture generator for ch17_gameboy_audio_accuracy.

Deterministic (no RNG/time). Writes into tests/hidden/ch17_gameboy_audio_accuracy/fixtures/:

  h1_square_sweep.apuprog   pulse 1, sweep + length + decay envelope
  h2_wave.apuprog           wave channel, custom RAM pattern, 50% code
  h3_noise.apuprog          noise bursts incl. rising envelope + width 7
  h4_coding_test.apuprog    the exact unseen configuration documented in
                            templates/ch17_gameboy_audio_accuracy/
                            99_coding_test/CODING_TEST.md

Format: little-endian records of u32 tcycleOffset, u16 regAddr, u8 value;
terminator record regAddr == 0xFFFF.
"""

import struct
import sys
from pathlib import Path

TERMINATOR = (0xFFFFFFFF, 0xFFFF, 0xFF)

HIDDEN_WAVE_RAM = [0x01, 0x23, 0x45, 0x67, 0x89, 0xAB,
                   0xCD, 0xEF, 0xFE, 0xDC, 0xBA, 0x98,
                   0x76, 0x54, 0x32, 0x10]

CODING_TEST_EVENTS = [
    (0, 0xFF26, 0x80), (0, 0xFF24, 0x77), (0, 0xFF25, 0xBF),
    (8, 0xFF11, 0x40), (8, 0xFF12, 0x74), (8, 0xFF13, 0xEE), (8, 0xFF14, 0x86),
    (16, 0xFF16, 0x80), (16, 0xFF17, 0x83), (16, 0xFF18, 0x22), (16, 0xFF19, 0x85),
    (24, 0xFF30, 0x01), (24, 0xFF31, 0x23), (24, 0xFF32, 0x45), (24, 0xFF33, 0x67),
    (24, 0xFF34, 0x89), (24, 0xFF35, 0xAB), (24, 0xFF36, 0xCD), (24, 0xFF37, 0xEF),
    (24, 0xFF38, 0xFE), (24, 0xFF39, 0xDC), (24, 0xFF3A, 0xBA), (24, 0xFF3B, 0x98),
    (24, 0xFF3C, 0x76), (24, 0xFF3D, 0x54), (24, 0xFF3E, 0x32), (24, 0xFF3F, 0x10),
    (24, 0xFF1A, 0x80), (24, 0xFF1C, 0x40), (24, 0xFF1D, 0x00), (24, 0xFF1E, 0x84),
    (32, 0xFF21, 0xF2), (32, 0xFF22, 0x27), (32, 0xFF23, 0x80),
]


def fnv1a(data: bytes) -> str:
    h = 0xCBF29CE484222325
    for b in data:
        h ^= b
        h = (h * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return f"{h:016X}"


def write_program(path: Path, events, description):
    blob = bytearray()
    for t, reg, val in events:
        if not (0xFF10 <= reg <= 0xFF3F):
            raise ValueError(f"bad register {reg:#06x}")
        blob += struct.pack("<IHB", t, reg, val)
    blob += struct.pack("<IHB", *TERMINATOR)
    path.write_bytes(blob)
    listing = [f"# {path.name} — register write table",
               f"# {description}",
               "# tcycle   reg   value",
               "# ------------------"]
    for t, reg, val in events:
        listing.append(f"  {t:<8d} {reg:#06x}  {val:#04x}")
    listing.append(f"  {'TERM':<8} {0xFFFF:#06x}  --")
    path.with_suffix(".asm.txt").write_text("\n".join(listing) + "\n")
    return fnv1a(blob)


def h1_square_sweep():
    ev = [(0, 0xFF26, 0x80), (0, 0xFF24, 0x57), (0, 0xFF25, 0x12),
          (0, 0xFF10, 0x24),   # pace 2, positive, slope 4
          (0, 0xFF11, 0xC0),   # 75% duty
          (0, 0xFF12, 0xB3)]   # volume 11, decay period 3
    ev += [(0, 0xFF13, 0xDC), (0, 0xFF14, 0x85)]        # freq 1500
    ev += [(130000, 0xFF13, 0xA4), (130000, 0xFF14, 0x86)]  # freq 1700
    # Third note climbs until the SECOND sweep update overflows and
    # silences the channel mid-note (accuracy probe).
    ev += [(260000, 0xFF13, 0x64), (260000, 0xFF14, 0x87)]  # freq 1900
    return ev


def h2_wave():
    ev = [(0, 0xFF26, 0x80), (0, 0xFF24, 0x66), (0, 0xFF25, 0x40),
          (0, 0xFF1A, 0x80), (0, 0xFF1C, 0x40)]
    for i, b in enumerate(HIDDEN_WAVE_RAM):
        ev.append((0, 0xFF30 + i, b))
    for t, f in [(10000, 300), (90000, 600), (160000, 1200)]:
        ev += [(t, 0xFF1D, f & 0xFF), (t, 0xFF1E, 0x80 | (f >> 8))]
    return ev


def h3_noise():
    ev = [(0, 0xFF26, 0x80), (0, 0xFF24, 0x77), (0, 0xFF25, 0x08)]
    ev += [(0, 0xFF21, 0xD6), (0, 0xFF22, 0x2C), (0, 0xFF23, 0x80)]
    ev += [(80000, 0xFF21, 0x83), (80000, 0xFF22, 0x14), (80000, 0xFF23, 0x80)]
    ev += [(150000, 0xFF21, 0xE0), (150000, 0xFF22, 0x3E), (150000, 0xFF23, 0x80)]
    return ev


def main():
    repo = Path(sys.argv[1]) if len(sys.argv) > 1 else Path.cwd()
    fixdir = repo / "tests" / "hidden" / "ch17_gameboy_audio_accuracy" / "fixtures"
    fixdir.mkdir(parents=True, exist_ok=True)
    jobs = [
        ("h1_square_sweep", h1_square_sweep(),
         "pulse 1: pace-2/slope-4 sweep, two notes then an "
         "overflow-disabling climb"),
        ("h2_wave", h2_wave(),
         "wave channel: checker-pattern RAM, 50% code, three frequency steps"),
        ("h3_noise", h3_noise(),
         "noise: decaying burst, RISING envelope burst, frozen width-7 burst"),
        ("h4_coding_test", CODING_TEST_EVENTS,
         "unseen coding-test configuration (see CODING_TEST.md)"),
    ]
    print("# fixture             FNV64")
    for name, ev, desc in jobs:
        h = write_program(fixdir / f"{name}.apuprog", ev, desc)
        print(f"{name:20s} {h}")


if __name__ == "__main__":
    main()
