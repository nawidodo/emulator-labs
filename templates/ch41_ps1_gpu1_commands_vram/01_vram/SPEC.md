# SPEC — Exercise 01: VRAM model and block transfers

Implement `psx::gpu::Vram` addressing plus the three PSX block-transfer
primitives in `vram.hpp`. Every rule below is quoted from PSX-SPX
("GPU Memory Transfer Commands", "Masking for COPY Commands parameters",
"Wrapping") — the hidden grader hashes VRAM images produced by these exact
semantics, so deviations are observable.

## Addressing

- VRAM = 1024 x 512 halfwords (1 MiB, 15-bit BGR555 pixels).
- `vram_index(x, y)`: X masked to 10 bits, Y masked to 9 bits. A pixel past
  column 1023 lands on column 0 of the SAME row; a row past 511 wraps to the
  top. There is never a carry-out from X to Y nor from Y to X.

## Sizes

All three COPY commands normalise their size fields:

```
Xsiz=((Xsiz-1) AND 3FFh)+1      ; range 1..400h
Ysiz=((Ysiz-1) AND 1FFh)+1      ; range 1..200h
```

A size field of 0 therefore means "maximum" (1024 x 512), not "nothing".
(FILL is different: see exercise 02.)

## Transfers

| Command | Direction | Notes |
|---|---|---|
| GP0(A0h) | CPU -> VRAM | source stream is linear w*h; destination wraps |
| GP0(C0h) | VRAM -> CPU | gather w*h row-major; popped via GPUREAD |
| GP0(80h) | VRAM -> VRAM | snapshot source first (HW latches 128-halfword chunks), so overlapping copies do not smear |

## Acceptance

`ch41_01_vram_tests` passes (RED on skeleton, GREEN when the five TODO
blocks are done). Wrap cases mirror DuckStation's transfer slow path:
`(x+col) % 1024`, `(y+row) % 512`.
