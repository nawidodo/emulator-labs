# Provenance — ch41 public fixtures and goldens

## Fixture

`roms/challenge_stream.bin` was assembled by this chapter's throwaway Python
assembler (see `tests/hidden/ch41_ps1_gpu1_commands_vram/provenance.md` for
the format rationale). Annotated listing: `roms/challenge_stream.asm.txt`.
It exercises: GP1 display setup (08h/03h/04h/05h), attribute commands
E3h/E4h/E5h/E6h, FILL with the width-rounding quirk (`3FFh` -> full-width
400h; literal `400h` would collapse to no-fill), a flat red triangle, a
Gouraud triangle, a monochrome quad, an azure variable-size rectangle, an
A0h upload whose rows wrap across column 1023, and an 80h VRAM->VRAM copy.
The stream starts with a register-neutralization prologue and is idempotent
under repetition (`--frames 8` hashes identically to `--frames 1`).

## Goldens

Generated with the reference solution runner, run twice per invocation mode
with byte-identical output:

```bash
ch41_91_challenge_runner \
    --rom tests/public/ch41_ps1_gpu1_commands_vram/roms/challenge_stream.bin \
    --frames 1 --trace goldens/challenge_trace.log \
    --hash-frame goldens/challenge_frame_hash.txt
# challenge_frame_hash.txt: fnv64=9DCE1D6B7B351C39   (full LE VRAM dump)
```

The trace logs one line per consumed word:
`pc=<byte-offset hex> op=<word hex> cyc=<n>`.

Hidden-side fixtures and their golden hashes are documented in
`tests/hidden/ch41_ps1_gpu1_commands_vram/provenance.md` and recorded in
that directory's `manifest.json`.
