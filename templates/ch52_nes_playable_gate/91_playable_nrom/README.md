# 91 — Playable NES (NROM)

Compose the verified course components into a playable whole-machine NES
that passes the gate in `tests/hidden/ch52_nes_playable_gate/`.

## CLI contract (graded via `LABS_NES_GATE_BIN`)

Your binary must implement:

    --rom <path>            iNES NROM ROM (16 KB PRG + 8 KB CHR)
    --input-file <path>     scripted controller input (one hex byte per
                            frame, `NNN XX` lines; accepted but may be
                            ignored — the homebrew is deterministic)
    --frames <N>            run N frames (required)
    --headless              no window
    --hash-frame <path>     write 256x240x4 RGBA8 frame of the last frame
    --audio-out <path>      write mono s16le PCM segment (one sample per
                            CPU cycle, ch24 contract)
    --gate <path>           write EMU_GATE_V1 checkpoint
    --selfcheck-replay      run twice, require byte-identical checkpoint

The reference runner is `tools/labs/nes_gate/nes_gate_runner` (built
automatically in solution mode). Study `tools/labs/nes_gate/EMU_GATE_V1.md`
for the checkpoint format and
`tests/public/ch52_nes_playable_gate/goldens/provenance.md` for provenance.

The public ROM is `tests/public/ch52_nes_playable_gate/roms/gate_homebrew.nes`
(a tiny hand-authored NROM demo) and `scripts/play.input` is the 180-line
script the hidden grade uses.

## Build

    cmake -S . -B build -DLABS_BUILD_SOLUTIONS=Off && cmake --build build --parallel 4
