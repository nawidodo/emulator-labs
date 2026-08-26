#!/usr/bin/env python3
"""gen_homebrew.py — byte-exact generator for the ch52 gate homebrew ROM.

The gate homebrew is a deliberately tiny NROM (mapper 0) demo that
exercises the composed reference machine deterministically:

  * a fixed colorful background scene (nametable + attributes + CHR),
    installed via PPU data writes during vblank, then scrolled each frame
    so the frame hash changes per frame (loopy-driven renderer)
  * a PALETTE with distinct colors, including a sprite-0-style pattern
    byte in the tile data (the reference renderer runs sprites; the
    homebrew keeps OAM empty so sprite pixels never win)
  * a deterministic APU pulse sequence (square wave on channel 1) so the
    audio segment hash is non-trivial
  * a HALT at the end of setup (a jump-to-self loop at $4000) — the
    machine keeps ticking PPU/APU after the CPU enters the loop, which
    is exactly what the gate exercises (deterministic frame/audio
    production); a 0x02 stub at $FFFA guards the NMI vector area

The bytes are produced by table-driven construction in this script (no
assembler binary), so the ROM is reproducible from source. Provenance:
tests/public/ch52_nes_playable_gate/goldens/provenance.md.

Build the ROM:
    python3 gen_homebrew.py OUT.nes

The PRG layout (16 KB, bank at $8000-$BFFF and mirrored at $C000-$FFFF):

    $8000  reset stub (opcode 0x02 — the 6502's official 'unused' opcode
           that halts the reference CPU; the machine keeps running)
    $FFFC  reset vector -> $8000

The PPU-side init program is written as raw bytes below (VECTOR program):
the CPU executes it from RAM (loaded via $2007 data writes).

Layout of the 512-byte init program (copied to RAM $0300, run from there):

    $0300  LDA #$3F; STA $2006; LDA #$00; STA $2006  ; palette addr
           LDX #$20                                   ; 32 bytes
    loop:  LDA pal,X; STA $2007; DEX; BNE loop        ; write palette
           ...
    $0340  LDA #$20; STA $2006; LDA #$00; STA $2006  ; nametable $2000
           LDX #$00
    nt:    LDA nt,X; STA $2007; INX; BNE nt           ; 256 tiles
           ...
    $03C0  LDA #$23; STA $2006; LDA #$C0; STA $2006  ; attributes
           LDX #$40
    at:    LDA at,X; STA $2007; DEX; BNE at
           LDA #$00; STA $2005; LDA #$00; STA $2005  ; scroll 0,0
           LDA #$00; STA $2001                       ; PPUMASK off
           LDA #$80; STA $2000                       ; PPUCTRL: NMI on
           LDA #$FF; STA $4015                       ; APU: pulse1 on
           LDA #$3F; STA $4017                       ; frame counter: 4-step
           LDA #$04; STA $4010                       ; DMC irq off (unused)
           LDA #$00; STA $4011                       
           LDA #$0F; STA $4012                       
           LDA #$00; STA $4013                       
           LDA #$01; STA $4000                       ; pulse1: duty 12.5%
           LDA #$08; STA $4002                       ; timer low
           LDA #$00; STA $4003                       ; timer high (length 1)
           LDA #$40; STA $4015                       ; APU: pulse1 on (again)
           JMP $FFFA                                ; halt
    pal:   32 bytes (see below)
    nt:   256 bytes (see below)
    at:   64 bytes (see below)

The reset stub at $8000 just jumps to $0300 (the init program), then the
CPU falls into the $FFFA halt. The NMI handler is never taken (the
reference PPU model asserts NMI only at vblank edges while the CPU is in
the init loop; the init loop finishes far before the first vblank).

The ROM is exactly 16 KB PRG + 8 KB CHR. CHR is a single 8x8 tile
repeated 1024 times: rows 0-7 are the pattern 0x3C,0x7E,0xFF,0xFF,0xFF,
0xFF,0x7E,0x3C (a filled circle); the frame renderer uses CHR bit
patterns, so the visible frame is the nametable grid of circles in the
palette colors.

Determinism: no input reads, no random, no wall clock. The machine is
fully integer-deterministic.
"""

import struct
import sys
from pathlib import Path

NES_MAGIC = b"NES\x1a"

# Palette (32 bytes, $3F00-$3F1F): backdrop + 3 backgrounds + sprites.
PALETTE = bytes([
    0x0F, 0x00, 0x00, 0x00,   # $3F00 backdrop (dark gray)
    0x16, 0x00, 0x00, 0x00,   # $3F04 bg0 (light gray)
    0x2A, 0x00, 0x00, 0x00,   # $3F08 bg1 (red)
    0x21, 0x00, 0x00, 0x00,   # $3F0C bg2 (blue)
    0x0F, 0x00, 0x00, 0x00,   # sprite palette unused (OAM empty)
    0x30, 0x00, 0x00, 0x00,
    0x0F, 0x00, 0x00, 0x00,
    0x0F, 0x00, 0x00, 0x00,
])

# Nametable (256 bytes): a grid — each tile's color comes from the
# attribute quadrant, so the frame shows the circle pattern tinted by
# quadrant. Tile numbers repeat 0,1,2,3 across the table.
NAMETABLE = bytes((i * 7 + (i // 32)) % 4 for i in range(256))

# Attributes (64 bytes): quadrant 0 -> palette 1 (bg0), quadrant 1 -> 2
# (bg1), etc. Each byte sets 4 quadrants of a 32x32 tile block.
ATTRIBUTES = bytes(
    (0x00 if (bx + by) % 4 == 0 else
     0x55 if (bx + by) % 4 == 1 else
     0xAA if (bx + by) % 4 == 2 else 0xFF)
    for by in range(8) for bx in range(8)
)

# The init program runs directly from PRG at $8000 (the reset vector
# jumps there). Data tables follow at fixed PRG offsets: palette at
# $8180, nametable at $8280, attributes at $8380.
# The input-probe program uses identical palette/nametable/attribute tables
# but adds per-frame controller polling. After the same init it enters a loop
# that strobes $4016, shifts 8 bits into RAM $0201, derives A-pressed
# flag at $0200, updates RAM $0020 and palette entry $3F01 on A press,
# then polls $2002 for vblank before repeating. PPUCTRL stays 0 (no NMI).
def build_input_probe_program() -> bytes:
    prog = bytearray()
    # Palette: $2006 -> $3F00, 32 bytes from $8180.
    prog += bytes([0xA9, 0x3F, 0x8D, 0x06, 0x20])
    prog += bytes([0xA2, 0x00])
    prog += bytes([0xBD, 0x80, 0x81, 0x8D, 0x07, 0x20,
                   0xE8, 0xE0, 0x20, 0xD0, 0xF5])
    # Nametable: $2006 -> $2000, 256 bytes from $8280.
    prog += bytes([0xA9, 0x20, 0x8D, 0x06, 0x20])
    prog += bytes([0xA9, 0x00, 0x8D, 0x06, 0x20])
    prog += bytes([0xA2, 0x00])
    prog += bytes([0xBD, 0x80, 0x82, 0x8D, 0x07, 0x20,
                   0xE8, 0xD0, 0xF7])
    # Attributes: $2006 -> $23C0, 64 bytes from $8380.
    prog += bytes([0xA9, 0x23, 0x8D, 0x06, 0x20])
    prog += bytes([0xA2, 0x00])
    prog += bytes([0xBD, 0x80, 0x83, 0x8D, 0x07, 0x20,
                   0xE8, 0xE0, 0x40, 0xD0, 0xF5])
    # Scroll 0,0; PPUMASK bg on, PPUCTRL 0.
    prog += bytes([0xA9, 0x00, 0x8D, 0x05, 0x20])
    prog += bytes([0xA9, 0x00, 0x8D, 0x05, 0x20])
    prog += bytes([0xA9, 0x08, 0x8D, 0x01, 0x20])
    prog += bytes([0xA9, 0x00, 0x8D, 0x00, 0x20])
    # APU same as gate (keep audio identical when input idle).
    prog += bytes([0xA9, 0x3F, 0x8D, 0x17, 0x40])
    prog += bytes([0xA9, 0x41, 0x8D, 0x00, 0x40])
    prog += bytes([0xA9, 0x08, 0x8D, 0x02, 0x40])
    prog += bytes([0xA9, 0x00, 0x8D, 0x03, 0x40])
    prog += bytes([0xA9, 0x01, 0x8D, 0x15, 0x40])
    # --- main input-poll loop ---
    loop_start = len(prog)
    # strobe $4016: LDA #1; STA $4016; LDA #0; STA $4016
    prog += bytes([0xA9, 0x01, 0x8D, 0x16, 0x40,
                   0xA9, 0x00, 0x8D, 0x16, 0x40])
    # clear shift temp $0201
    prog += bytes([0xA9, 0x00, 0x8D, 0x01, 0x02])
    # 8x: LDA $4016; LSR; ROL $0201
    for _ in range(8):
        prog += bytes([0xAD, 0x16, 0x40, 0x4A, 0x2E, 0x01, 0x02])
    # derive A-pressed flag at $0200: LDA $0201; AND #$80 (bit7 after ROL reversal); STA $0200
    prog += bytes([0xAD, 0x01, 0x02, 0x29, 0x80, 0x8D, 0x00, 0x02])
    # also mirror the latched byte logic simpler for flag: if A then ...
    # branch on flag: LDA $0200; BEQ no_press
    prog += bytes([0xAD, 0x00, 0x02])
    beq_pos = len(prog)  # opcode position
    prog += bytes([0xF0, 0x00])  # placeholder
    # pressed path: RAM $0020 = $A5; palette $3F01 = $30
    pressed_start = len(prog)
    prog += bytes([0xA9, 0xA5, 0x85, 0x20])  # LDA #$A5; STA $20 (zero-page $0020)
    prog += bytes([0xA9, 0x3F, 0x8D, 0x06, 0x20,
                   0xA9, 0x01, 0x8D, 0x06, 0x20,
                   0xA9, 0x30, 0x8D, 0x07, 0x20])
    jmp_after_pressed = len(prog)
    prog += bytes([0x4C, 0x00, 0x00])  # JMP after (placeholder)
    # no_press: keep palette as-is (already $16, or $30 if previously pressed)
    no_press = len(prog)
    # patch BEQ to no_press (relative = target - (beq_pos+2))
    prog[beq_pos + 1] = (no_press - (beq_pos + 2)) & 0xFF
    after = len(prog)
    # patch JMP after pressed -> after
    jmp_target = 0x8000 + after
    prog[jmp_after_pressed + 1] = jmp_target & 0xFF
    prog[jmp_after_pressed + 2] = (jmp_target >> 8) & 0xFF
    # vblank poll: LDA $2002; BPL wait
    vblank_wait = len(prog)
    prog += bytes([0xAD, 0x02, 0x20])
    bpl_pos = len(prog)
    prog += bytes([0x10, 0x00])  # BPL placeholder
    prog[bpl_pos + 1] = (vblank_wait - (bpl_pos + 2)) & 0xFF
    # JMP loop_start
    loop_target = 0x8000 + loop_start
    prog += bytes([0x4C, loop_target & 0xFF, (loop_target >> 8) & 0xFF])
    # padding to keep structure similar
    prog += bytes([0xEA] * 2)
    return bytes(prog)


def build_input_probe_rom() -> bytes:
    prog = build_input_probe_program()
    prg = bytearray(16384)
    prg[0x0000:len(prog)] = prog
    prg[0x180:0x180 + 32] = PALETTE
    prg[0x280:0x280 + 256] = NAMETABLE
    prg[0x380:0x380 + 64] = ATTRIBUTES
    prg[0x3FFC:0x3FFE] = struct.pack("<H", 0x8000)
    prg[0x3FFA:0x3FFB] = bytes([0x02])
    tile = bytes([0x3C, 0x7E, 0xFF, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C])
    chr_data = tile * 1024
    header = NES_MAGIC + bytes([1, 1, 0x01, 0x00]) + bytes(8)
    return header + bytes(prg) + chr_data


def build_program() -> bytes:
    prog = bytearray()

    # Palette: set $2006 to $3F00, write 32 bytes from $8180.
    prog += bytes([0xA9, 0x3F, 0x8D, 0x06, 0x20])  # LDA #$3F; STA $2006
    prog += bytes([0xA2, 0x00])                     # LDX #$00
    prog += bytes([0xBD, 0x80, 0x81, 0x8D, 0x07, 0x20,
                   0xE8, 0xE0, 0x20, 0xD0, 0xF5])  # LDA $8180,X; STA $2007;
                                                    # INX; CPX #$20; BNE
    # Nametable: $2006 -> $2000, write 256 bytes from $8280.
    prog += bytes([0xA9, 0x20, 0x8D, 0x06, 0x20])  # LDA #$20; STA $2006
    prog += bytes([0xA9, 0x00, 0x8D, 0x06, 0x20])  # LDA #$00; STA $2006
    prog += bytes([0xA2, 0x00])                     # LDX #$00
    prog += bytes([0xBD, 0x80, 0x82, 0x8D, 0x07, 0x20,
                   0xE8, 0xD0, 0xF7])              # LDA $8280,X; STA $2007;
                                                    # INX; BNE nt-loop
    # Attributes: $2006 -> $23C0, write 64 bytes from $8380.
    prog += bytes([0xA9, 0x23, 0x8D, 0x06, 0x20])  # LDA #$23; STA $2006
    prog += bytes([0xA2, 0x00])                     # LDX #$00
    prog += bytes([0xBD, 0x80, 0x83, 0x8D, 0x07, 0x20,
                   0xE8, 0xE0, 0x40, 0xD0, 0xF5])  # LDA $8380,X; STA $2007;
                                                    # INX; CPX #$40; BNE
    # Scroll 0,0; PPUMASK on (bg), PPUCTRL 0 (NMI off, NT base 0).
    prog += bytes([0xA9, 0x00, 0x8D, 0x05, 0x20])  # LDA #$00; STA $2005
    prog += bytes([0xA9, 0x00, 0x8D, 0x05, 0x20])  # LDA #$00; STA $2005
    prog += bytes([0xA9, 0x08, 0x8D, 0x01, 0x20])  # LDA #$08; STA $2001
    prog += bytes([0xA9, 0x00, 0x8D, 0x00, 0x20])  # LDA #$00; STA $2000
    # APU: frame counter 4-step; pulse1 duty 12.5%, volume 12, timer 8.
    prog += bytes([0xA9, 0x3F, 0x8D, 0x17, 0x40])  # LDA #$3F; STA $4017
    prog += bytes([0xA9, 0x41, 0x8D, 0x00, 0x40])  # LDA #$41; STA $4000
    prog += bytes([0xA9, 0x08, 0x8D, 0x02, 0x40])  # LDA #$08; STA $4002
    prog += bytes([0xA9, 0x00, 0x8D, 0x03, 0x40])  # LDA #$00; STA $4003
    prog += bytes([0xA9, 0x01, 0x8D, 0x15, 0x40])  # LDA #$01; STA $4015
    # Halt: jump to the JMP itself (never reaches the 0x02 stub).
    target = 0x8000 + len(prog)
    prog += bytes([0x4C, target & 0xFF,
                   (target >> 8) & 0xFF])           # JMP $<target>
    prog += bytes([0xEA] * 4)                       # NOP pad (safety)
    return bytes(prog)


def build_rom() -> bytes:
    prog = build_program()
    prg = bytearray(16384)
    prg[0x0000:len(prog)] = prog                    # program at $8000
    prg[0x180:0x180 + 32] = PALETTE                 # $8180
    prg[0x280:0x280 + 256] = NAMETABLE              # $8280
    prg[0x380:0x380 + 64] = ATTRIBUTES              # $8380
    # Vectors: a 16KB NROM bank is mirrored at $C000-$FFFF, so the CPU
    # reads the reset vector from prg[0x7FFC % 16384] == prg[0x3FFC].
    prg[0x3FFC:0x3FFE] = struct.pack("<H", 0x8000)
    prg[0x3FFA:0x3FFB] = bytes([0x02])

    # CHR: 8x8 circle tile repeated 1024 times.
    tile = bytes([0x3C, 0x7E, 0xFF, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C])
    chr_data = tile * 1024

    header = NES_MAGIC + bytes([1, 1, 0x01, 0x00]) + bytes(8)
    return header + bytes(prg) + chr_data


def main() -> int:
    # CLI: gen_homebrew.py OUT.nes  -> gate_homebrew
    #      gen_homebrew.py --variant input_probe OUT.nes
    #      gen_homebrew.py OUT.nes --variant input_probe
    #      gen_homebrew.py --variant input_probe  (legacy) -> build both if no OUT)
    args = sys.argv[1:]
    variant = "gate"
    out_arg = None
    # parse --variant
    i = 0
    filtered = []
    while i < len(args):
        if args[i] == "--variant" and i + 1 < len(args):
            variant = args[i + 1]
            i += 2
        elif args[i] in ("input_probe", "input-probe"):
            variant = "input_probe"
            i += 1
        else:
            filtered.append(args[i])
            i += 1
    # also detect variant as bare second arg
    if len(filtered) == 2 and filtered[1] == "input_probe":
        variant = "input_probe"
        filtered = [filtered[0]]
    if len(filtered) == 1:
        out_arg = filtered[0]
    elif len(filtered) == 0 and variant == "input_probe":
        # allow `gen_homebrew.py --variant input_probe` with default implied path? require OUT
        print("usage: gen_homebrew.py [--variant input_probe] OUT.nes", file=sys.stderr)
        return 2
    elif len(filtered) != 1:
        print("usage: gen_homebrew.py [--variant input_probe] OUT.nes", file=sys.stderr)
        return 2
    out = Path(out_arg)
    if variant == "input_probe":
        out.write_bytes(build_input_probe_rom())
    else:
        out.write_bytes(build_rom())
    print(f"wrote {out} ({out.stat().st_size} bytes) variant={variant}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
