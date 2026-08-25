# Provenance — ch41 hidden fixtures

All fixtures are synthetic GP0/GP1 command streams assembled by this
chapter's throwaway Python assembler; each has an annotated `.asm.txt`
listing next to it. Stream format: raw little-endian u32 words interpreted
as GP0 writes; the reserved-GP0-opcode escape word `12000000h` introduces a
raw GP1 command. Every stream begins with a register-neutralization
prologue (masks off, full drawing area, zero drawing offset) so re-running
the stream is deterministic.

| fixture | exercises |
|---|---|
| `xfer_roundtrip.bin` | A0h upload whose rows wrap across column 1023, 80h copy of the wrapped block, small upload, C0h download |
| `fill_quirk.bin` | FILL parameter masking/rounding quirks: literal 400x512 collapses to no-fill; 3FFh rounds up to full width; Xpos alignment to 16; vertical wrap |
| `gouraud_golden.bin` | check-mask stripe preservation under a rect, boundary-crossing flat + Gouraud triangles against drawing area/offset, interior quad |
| `tri_unseen.bin` | CODING TEST unseen triangle state (see templates/ch41_ps1_gpu1_commands_vram/99_coding_test/CODING_TEST.md) |

Golden hashes were produced by the reference solution runners (91_challenge /
99_coding_test), run twice each with byte-identical output:

```bash
ch41_91_challenge_runner --rom tests/hidden/ch41_ps1_gpu1_commands_vram/roms/<fixture>.bin \
    --frames 1 --hash-frame out.txt   # fnv64 over the full LE VRAM dump
```

Values recorded in `manifest.json` are FNV-1a-64 over the exact bytes of
the `--hash-frame` output file (`fnv64=<HEX>\n`), matching
`tools/labs/grade.py` semantics. The ps1-tests suite
(https://github.com/avocado-ps/ps1-tests) is referenced by URL only via an
optional `requires_rom` manifest entry; exercising those binaries needs the
ch38 CPU and an ELF loader, which is outside this chapter's scope.
