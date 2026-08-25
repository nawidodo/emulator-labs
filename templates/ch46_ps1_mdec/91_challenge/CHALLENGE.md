# Challenge — ch46: DMA-fed MDEC frame decode

Hardware streams MDEC data through channel 1 as a river of 32-bit words;
decoded RGB15 leaves through channel 0. This challenge reproduces that at
lab scale and closes the loop with an exact pixel hash.

## Your task

`mdec_core.hpp` wires the three exercises into a pipeline:

```
DmaFeed (u32 words -> u16 units)
  -> decode_block (RLZ + dequantize + de-zigzag)
  -> idct8x8
  -> assemble_macroblock (YUV->RGB15 + x2 chroma upscale)
```

Extend/harden it so that:

1. Multi-macroblock frames decode in stream order; each contributes
   exactly 256 row-major RGB15 words.
2. Truncated or malformed streams stop cleanly (no crashes, no overrun).
3. Output is deterministic across runs and machines — same stream,
   same FNV-1a-64 hash.

## Acceptance

```bash
ch46_91_mdec_tests                       # unit tests GREEN
ch46_mdec_cli \
    --stream tests/public/ch46_ps1_mdec/streams/pub.bin \
    --out pub.rgba15 --hash-out pub_hash.txt
# pub_hash.txt matches tests/public/ch46_ps1_mdec/streams/pub_hash.txt
# (fnv64 = 6D0D5B30487ECA5, see ../tests/public/ch46_ps1_mdec/provenance.md)
```

The hidden grader decodes an unseen stream and compares hashes — one
wrong coefficient placement or clamp breaks it.
