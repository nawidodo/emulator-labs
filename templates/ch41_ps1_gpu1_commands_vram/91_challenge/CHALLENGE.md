# Challenge — render a GP0 command stream to a golden VRAM image

You have the complete reference GPU (this directory's `gpu.hpp`, no stubs).
Your job is to *use* it and verify it against committed goldens.

## Acceptance criteria

1. `ch41_91_challenge_runner --rom tests/public/ch41_ps1_gpu1_commands_vram/roms/challenge_stream.bin --frames 1 --hash-frame /tmp/h.txt`
   exits 0 and `/tmp/h.txt` contains exactly the hash recorded in
   `tests/public/ch41_ps1_gpu1_commands_vram/goldens/challenge_frame_hash.txt`.
2. `--frames 8` produces the SAME hash as `--frames 1` for this fixture:
   every command in the stream is idempotent, which is itself a property
   worth understanding (which command class would NOT be idempotent under
   repetition? think VRAM->VRAM copies of overlapping regions).
3. `--trace` output matches
   `tests/public/ch41_ps1_gpu1_commands_vram/goldens/challenge_trace.log`
   line for line.
4. Unknown flags exit nonzero with usage on stderr; `--help` exits 0.

## What the fixture exercises

Display setup (GP1 03h/04h/05h/08h), drawing area + offset + mask attributes
(E3h..E6h), full-screen FILL with the width-quirk parameter (`3FFh`
rounding up because literal `400h` collapses to zero — see LECTURE.md),
a flat triangle, a Gouraud triangle, a monochrome quad, a variable-size
rectangle, a CPU->VRAM upload that wraps across the right VRAM edge, and a
VRAM->VRAM copy.

## Provenance

Fixture bytes, disassembly listing and generating commands are documented
in `tests/public/ch41_ps1_gpu1_commands_vram/provenance.md`.
