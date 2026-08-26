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

## Input

`scripts/play.input` is a deterministic 180-line script (one controller
byte per frame, hex, cycling a fixed 10-entry pattern XORed with the frame
index). The gate homebrew ignores input reads (controller polling is
optional), so the script is accepted by the reference runner but does not
affect the hashes — it exists to prove the per-frame latching plumbing.

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
