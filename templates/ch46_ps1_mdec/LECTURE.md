# Chapter 46 — MDEC Video Decoder

The MDEC (Motion DECoder) is a fixed-function JPEG-like decoder: DMA
streams quantized coefficients in, RGB15 macroblocks come out. Games use
it for full-motion video and pre-rendered backdrops. It has no command
list and no registers beyond status — just two DMA channels and math that
must be reproduced exactly or video shimmers.

## Pipeline

```
DMA words -> RLZ decode -> dequantize -> IDCT -> YCbCr->RGB15 -> DMA out
```

A macroblock is six 8x8 blocks: four luma (Y0..Y3 covering 16x16) plus
one Cb and one Cr plane (upsampled x2).

## RLZ bitstream

Each compressed block starts with one 16-bit header unit:

```
bit  15    Q table select (this lab: flat tables either way)
bits 14:0  quantizer scale
```

followed by run/level units:

```
bits 15:10  run of zero coefficients (zig-zag order)
bits  9:0   signed level (10-bit two's complement)
FE00        end of block
```

Dequantization truncates toward zero (psx-spx):

```
DC (position 0): value = level * scale * Q[0] / 8
AC (position p): value = level * scale * Q[p] / 16
```

Coefficients are stored in ZIG-ZAG order and must be un-scanned into
natural order via the standard 64-entry table. Applying the scan twice —
or not at all — produces the famous "diagonal garbage" video bug seeded
in this chapter's debug exercise.

## IDCT

This lab uses a separable integer IDCT with a committed basis matrix
(round(c(u)*cos((2x+1)u*pi/16)*32), generated once so all platforms are
bit-identical):

```
pass 1: t[y][x] = (sum_u F[y][u] * M[x][u]) >> 5
pass 2: o[y][x] = (sum_v t[v][x] * M[y][v] + 64) >> 7
```

The total shift undoes the basis scale and the DCT 1/4 factor; +64 gives
round-to-nearest on the final pass per psx-spx rounding notes.

## Color conversion

```
r = y + ((1436*cr + 512) >> 10)
g = y - ((352*cb + 731*cr + 512) >> 10)
b = y + ((1815*cb + 512) >> 10)
```

clamped to 0..255 — never wrapped — then packed BGR555:
`(b>>3)<<10 | (g>>3)<<5 | (r>>3)`.

## DMA framing

MDECout words carry two 16-bit units each (high half first). The lab's
`DmaFeed` models this FIFO; the challenge decodes whole frames through it
and hashes the RGB15 output with FNV-1a-64 — the same digest hidden
manifests compare.
