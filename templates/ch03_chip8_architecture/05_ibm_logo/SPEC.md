# SPEC — Challenge: the IBM-style logo test

The classic CHIP-8 rite of passage is rendering the IBM logo. This course
ships a **course-original equivalent**: a hand-assembled program (no
commercial or third-party ROM bytes) that draws the word `CHIP` with four
8x8 glyphs using only this chapter's instruction set.

## Program

`roms/ibm_logo.ch8`, 62 bytes — annotated listing in
`roms/ibm_logo.asm.txt`. It uses exactly: `00E0 CLS`, `6XNN LD Vx`,
`ANNN LD I`, `DXYN DRW`, `7XNN ADD Vx`, `1NNN JP` (as a final self-jump so
the runner parks deterministically).

## Acceptance

1. Build the exercise and run its unit suite (`ch03_05_logo`) — all green,
   including `challenge.ibm_style_logo_frame_is_stable`.
2. Headless render matches the committed golden frame:

   ```bash
   ./ch03_05_logo_runner \
       --rom tests/public/ch03_chip8_architecture/roms/ibm_logo.ch8 \
       --headless --cycles 20 --hash-frame /tmp/logo.rgba
   python3 tools/labs/hash_frame.py /tmp/logo.rgba
   ```

   The FNV-1a 64 digest must equal the one recorded in
   `tests/public/ch03_chip8_architecture/provenance.md`.

## Hints (curriculum difficulty curve says you get some)

- If the screen stays black, check that `I` actually points at glyph data
  (`LD I` before every `DRW`) and that your fetch is big-endian.
- If glyphs smear horizontally, your sprite column loop walks bits in the
  wrong order: MSB first, `0x80 >> bit`.
- If everything doubles/erases, VF collision handling must XOR, not OR.
- Compare a trace against `tests/public/ch03_chip8_architecture/traces/`
  with `tools/labs/compare_trace.py` and find the FIRST divergence.
