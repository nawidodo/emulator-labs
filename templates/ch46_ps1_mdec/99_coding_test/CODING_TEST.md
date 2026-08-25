# Coding test — ch46: decode an unseen compressed block to an exact hash

Implement the FULL MDEC pipeline from this specification alone. Hidden
grading feeds your build a compressed macroblock stream you have never
seen and compares a 64-bit FNV-1a hash of the output pixels.

## Pipeline spec

1. **Stream container.** The file is a byte string; interpret it as a
   sequence of big-endian 16-bit values (DMA words pack two units each,
   high half first). A macroblock is six blocks: Y0 Y1 Y2 Y3 Cb Cr.
   Each block starts with one u16 `nunits`, followed by `nunits` u16 RLZ
   units.

2. **RLZ decoding** per block:
   - unit 0: bit 15 = Q-table select, bits 14..0 = quantizer scale;
   - data units: bits 15..10 = run of zeros, bits 9..0 = signed level
     (10-bit two's complement); value FE00 = end of block;
   - coefficients live in ZIG-ZAG order; use the standard scan table;
   - dequantization truncates toward zero:
     DC: `level * scale * Q[0] / 8`, AC: `level * scale * Q[p] / 16`;
   - all Q tables in this test are flat 16.

3. **IDCT.** Any exact separable integer IDCT is acceptable — the hidden
   blocks use ONLY the DC coefficient, so any correct IDCT yields the
   identical flat spatial block (`value` per sample).

4. **Color conversion** (psx-spx constants, rounding bias +512 before
   each >>10, clamp to 0..255):
   ```
   r = y + ((1436*cr) >> 10)
   g = y - ((352*cb + 731*cr) >> 10)
   b = y + ((1815*cb) >> 10)
   ```
   Pack BGR555: `(b>>3)<<10 | (g>>3)<<5 | (r>>3)`. Chroma upscales x2 by
   nearest neighbor across the 16x16 luma area.

5. **Hash.** Concatenate every macroblock's 256 RGB15 words row-major,
   little-endian, and compute FNV-1a 64 (offset CBF29CE484222325, prime
   100000001B3).

## Deliverable

```bash
./ch46_ct_pixel_tests --stream FILE --expect-hash HEX
# exits 0 iff the decoded pixel hash equals HEX (uppercase, no 0x)
```
