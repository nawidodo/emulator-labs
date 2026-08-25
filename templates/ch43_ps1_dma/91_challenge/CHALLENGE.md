# Challenge — ch43: GPU command lists through linked-list DMA

Real games render by building a linked list of GPU packet buffers in RAM,
kicking channel 2 in linked-list mode, and letting the DMA unit stream
packets word-by-word into the GP0 FIFO. This challenge reproduces that
pipeline end-to-end at lab scale.

## Your task

`challenge.hpp` + `challenge_runner.cpp` already wire a `MiniGpu` to the
chapter-3 list walker. The mini-GPU understands one primitive:

```
FillRect: cmd word [31:24]=02h, [15:0]=BGR15 color
          word 1 = x<<16 | y        (signed 16-bit, clamped)
          word 2 = width<<16 | height
```

Unknown opcodes consume their declared words and are ignored (like real
GPU command parsing, which never desynchronizes on unknown IDs).

Extend or harden the pipeline so that:

1. A chain mixing FillRect and unknown-opcode packets renders correctly
   (unknown packets must not desync the parser).
2. Draw order follows chain order — later packets overwrite earlier ones
   inside overlap regions.
3. The final VRAM (64x32 RGB15) hashes deterministically:
   `fnv64=` printed by the runner must be stable across runs and machines.
4. Malformed chains (no sentinel) trip the walker safety cap instead of
   looping forever (exit code 3).

## Acceptance

```bash
ch43_91_challenge_tests                 # unit tests GREEN
ch43_91_challenge_runner \
    --list tests/public/ch43_ps1_dma/vram/pub.list \
    --vram-out pub_vram.bin --hash-out pub_hash.txt
# pub_vram.bin matches tests/public/ch43_ps1_dma/vram/pub_vram.bin
# (FNV-1a 64 = 58E5A86D6484744D, see provenance.md)
```

The hidden grader runs the same runner against an unseen list and compares
the hash.
