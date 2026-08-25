# Coding test — ch44: implement GTE opcode NCLIP from specification

You get the spec below and a stub (`nclip.hpp`). Hidden grading runs your
build against screen-coordinate triples you have never seen.

## Specification — NCLIP (COP2 command 04h)

NCLIP computes the 2D cross product of the triangle stored in the screen
coordinate FIFO (SXY0/SXY1/SXY2, data regs 12/13/14; each word packs SX
in the low half, SY in the high half):

```
MAC0 = SX0*SY1 + SX1*SY2 + SX2*SY0
     - SX0*SY2 - SX1*SY0 - SX2*SY1
```

Rules:

1. Products are computed in WIDE math (int64); MAC0 is the architectural
   32-bit result.
2. If the wide sum falls outside signed 32-bit range: clamp MAC0 to
   0x7FFFFFFF / -0x80000000 and set BOTH FLAG bits 21 (positive overflow)
   and 20 (negative overflow).
3. FLAG write semantics: keep the previous SF/LM echo bits (17/16), OR in
   the new MAC0 overflow bits, set bit 31 when any error bit is present,
   and mirror the upper 16-bit half into the lower half. With no errors,
   bit 31 stays clear.
4. Store MAC0 into data register 24.

A positive MAC0 means clockwise winding (backface), negative means the
triangle faces the camera — that is how games cull polygons.

## Deliverable

```bash
./ch44_ct_nclip_tests            # public unit tests GREEN
./ch44_ct_nclip_tests INPUT EXPECTED
# fixture mode: INPUT lines of "sx0 sy0 sx1 sy1 sx2 sy2" (decimal),
# EXPECTED lines of "mac0=<dec> flag=<HEX8>"; exits 0 iff every line
# matches exactly.
```
