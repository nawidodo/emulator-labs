# ch52 gate homebrew — provenance

## ROM

`roms/gate_homebrew.nes` is hand-authored in `tools/labs/nes_gate/gen_homebrew.py`
(the only source of truth). Regenerate byte-exactly with:

    python3 tools/labs/nes_gate/gen_homebrew.py tests/public/ch52_nes_playable_gate/roms/gate_homebrew.nes

Layout: NROM (mapper 0), 16 KB PRG at $8000, 8 KB CHR. Program at $8000
writes 32 palette bytes from $8180, 256 nametable bytes from $8280, and
64 attribute bytes from $8380 into PPU RAM via $2006/$2007, sets scroll
0,0, enables background rendering ($2001=0x08), and sets up pulse channel
1 ($4000/$4002/$4003/$4015) before entering a JMP-to-self halt. CHR is a
single 8x8 circle tile repeated 1024 times; the visible frame is the
nametable grid of circles tinted by the attribute quadrants. Reset vector
at $FFFC → $8000; NMI at $FFFA holds a `0x02` stub (never taken: PPUCTRL
NMI is left cleared at init, so the game is polled, not interrupt-driven).

`sha256sum` of the ROM at commit time:

    8d8826fe5477d35b241dfad278219e26d07ffc0a844f9b9618ebce1e153bb070

`roms/input_probe.nes` is the input-observable variant, built from the same
tables but with a per-frame controller latch. Regenerate with:

    python3 tools/labs/nes_gate/gen_homebrew.py --variant input_probe tests/public/ch52_nes_playable_gate/roms/input_probe.nes

The probe does the same palette/NT/attribute init, then loops:
strobe $4016 (`LDA #1; STA $4016; LDA #0; STA $4016`), shift 8 bits via
`LDA $4016; LSR; ROL $0201` into RAM $0201, derives A-pressed flag at
$0200, and on A sets `RAM[0x20]=0xA5` plus palette entry 1 `$3F01=$30`
(else leaves palette at the init `$16`). Each iteration polls `$2002`
(`LDA $2002; BPL wait`) for vblank before the next input latch.

`sha256sum` of `input_probe.nes` at commit time:

    7a4e40d1717a5dd592e858baf6745b4e1abb58acb14d19c31fe84f6d93cac24d

## Input

`scripts/play.input` is a deterministic 180-line script (one controller
byte per frame, hex, cycling a fixed 10-entry pattern XORed with the frame
index). The gate homebrew ignores input reads (controller polling is
optional), so the script is accepted by the reference runner but does not
affect the hashes — it exists to prove the per-frame latching plumbing.

`scripts/press_a.input` is a 120-line controller script where only frame 60
has A pressed (`01`), all others `00`:

    000 00 .. 059 00, 060 01, 061 00 .. 119 00

The probe ROM observes this: after frame 60, `RAM[0x20]=0xA5` and palette
entry 1 becomes `$30`, so `RAM_FNV` and `FRAME_FNV` differ versus an
all-zero control run.

## Goldens

`goldens/gate_reference.emu_gate` is the canonical EMU_GATE_V1 checkpoint
after `--frames 180` through the reference runner (`tools/labs/nes_gate/`).
Minted by:

    tools/labs/nes_gate/nes_gate_runner --rom roms/gate_homebrew.nes \
        --frames 180 --gate goldens/gate_reference.emu_gate \
        --hash-frame /tmp/f.rgba --audio-out /tmp/a.pcm

    # determinism: run twice, require byte-identical checkpoint
    cmp <(runner --gate a --hash-frame …) <(runner --gate b …)

At mint time:

    ROM_FNV  = 6C3267EBDFD4980F
    FRAME    = 180
    CPU_PC   = 8066    # halt self-loop at $8066 (JMP-to-self in PRG)
    FRAME_FNV (f.rgba) = 9907513AFD961865
    AUDIO_FNV (whole-run PCM, one s16le sample per CPU cycle)
             = 8B9F456C57B82565
    PPU_FNV  = 2A8156B66A60ED44
    RAM_FNV  = 28C31CF8DF2EC325

The hidden grade pins only `FRAME_FNV` and `AUDIO_FNV` of the emitted
`f.rgba` / `a.pcm` files as `expect_file_hash { fnv64 }`. The runner's
`--selfcheck-replay` compares full checkpoints byte-for-byte.

The controller case `gate_controller_script` pins the whole EMU_GATE_V1
checkpoint for `--rom input_probe.nes --input-file press_a.input --frames 120`:

    nes_gate_runner --rom roms/input_probe.nes --input-file scripts/press_a.input --frames 120 --gate controller.emu_gate

    # determinism: two runs byte-identical
    # no-input control with all-zero script has different RAM_FNV/FRAME_FNV

## validated_against

Correctness is not determinism alone. Each artifact was validated against an
independent oracle before minting:

- **CPU** — `ch19` trusted 6502 tests (nestest-style traces, `ch19_03_trace_log`); every opcode/vector/NMI edge matched the reference trace.
- **NROM** — hand-proved address mapping (mirror_translate proof, PRG/CHR split, iNES header); PRG at $8000 mirrored at $C000, CHR 8 KB, mirroring vertical/horizontal.
- **PPU register writes** — NESdev-defined `$2006`/`$2007`/`$2005` semantics; `$2006` sets `v`, `$2007` writes at `v` then `v+=1`, `$2005` sets scroll `x`/`t`.
- **frame** — independent expected-frame generator (20-line python that re-derives nametable/palette/CHR → RGBA and hashes it); the generator reconstructs the same `FRAME_FNV` without the runner:

```python
# independent frame oracle (no runner imports)
PAL = bytes([0x0F,0x16,0x2A,0x21])  # + mirrors, 32B
NT  = bytes((i*7+(i//32))%4 for i in range(256))
AT  = bytes((0x00 if (bx+by)%4==0 else 0x55 if (bx+by)%4==1 else 0xAA if (bx+by)%4==2 else 0xFF) for by in range(8) for bx in range(8))
CHR = bytes([0x3C,0x7E,0xFF,0xFF,0xFF,0xFF,0x7E,0x3C])*1024
# map tile (0-3) + quadrant palette to RGB via NES palette LUT, render 256x240, fnv1a hash
def fnv(b): h=0xCBF29CE484222325; p=0x100000001B3; [exec('h^=c;h=(h*p)&0xFFFFFFFFFFFFFFFF',{'h':h,'c':c,'p':p}) for c in b]; return h
# (full loop in goldens/expected_frame.py — abbreviated here for provenance)
```

- **audio** — hand-proved pulse-channel vector (period/duty/length → sample formula); `ApuLite::mix()*512` matches `duty_tab[2]=0x3F` style reasoning.
- **DMA/timing** — existing `ch24` cycle tests (`nes24sync` frame counter, OAM DMA 513/514 cycles, `kPpuDotsPerCpu=3`).

All oracles passed before minting. See also `goldens/manifest.json` for machine-readable metadata.
