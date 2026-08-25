# Challenge — Boot the SI-Compatible Diagnostic

`ch09_si_diag.bin` (163 bytes, listing and provenance in
`tests/public/ch09_space_invaders_machine/roms/`) is a course-original
hand-assembled diagnostic that exercises the whole machine at once:

- **Ports:** OUT 2 latches shift amount 3, OUT 4 fills the shifter
  (low `0xDE`, high `0xAD`), IN 3 reads the window back, IN 1 samples a
  scripted fire button, OUT 3 / OUT 5 / OUT 6 hit the sound stubs and the
  watchdog.
- **Self-check:** the program compares the observed IN 3 value against
  the expected `0xBB` and stores its own verdict byte (01 = pass) in RAM.
- **Interrupts:** counting handlers on RST 08 / RST 10 record the frame
  cadence into RAM; the RST 10 handler also paints each count into VRAM,
  tying the final image to exact interrupt timing.
- **Video:** the painted VRAM produces a deterministic final frame.

## Acceptance criteria

```bash
ctest --test-dir build -R ch09_91_challenge        # all green
```

and via the headless runner against the committed fixture:

```bash
./build/skels/ch09_space_invaders_machine/91_challenge/ch09_91_si_runner \
    --rom tests/public/ch09_space_invaders_machine/roms/ch09_si_diag.bin \
    --frames 6 --input-file tests/public/ch09_space_invaders_machine/input/ch09_si_diag.inputs \
    --hash-frame /tmp/diag.rgba
# AF=0046 BC=0000 DE=0000 HL=0000 SP=2100 PC=007E cyc=161702
```

Acceptance = final RAM state matches AND the FNV-64 of the dumped frame
matches the golden recorded in the hidden manifest. Use
`tools/labs/hash_frame.py /tmp/diag.rgba` to inspect your digest.

## Going further

Real Space Invaders ROM sets run beautifully once this challenge passes;
they are student-supplied commercial images and never enter this repo.
