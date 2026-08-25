# Chapter 44 — Geometry Transformation Engine

The GTE is a coprocessor (COP2) next to the MIPS CPU that transforms
vertices: rotation, perspective projection, lighting and depth cueing.
Games push every polygon through it, so its arithmetic — fixed point,
saturation and flag semantics — must be reproduced EXACTLY or geometry
wobbles, polygons vanish and lighting flickers.

## COP2 register spaces

128 data-space words and 32 control words, accessed with `mfc2`/`mtc2`:

```
data    0/1  VXY0/VZ0     vertex 0 (VX low half, VY high half / VZ)
data   12/13 SXY0/SXY1    screen FIFO
data   14    SXY2         newest screen coordinates
data   17-19 SZ1..SZ3     depth FIFO
data   24-27 MAC0..MAC3   multiply-accumulator results
ctrl    0-4   R11..R33    rotation matrix, two 1.3.12 lanes per word
ctrl    5-7   TRX/TR Y/TRZ translation, 1.19.12
ctrl    8-12  L11..L33    light matrix
ctrl   13/14  RBK/GBK/BBK background color
ctrl   15-20  LR/LG/LB    light-source color matrix
ctrl   21-23  RFC/GFC/BFC far color
ctrl   24-26  OFX/OFY/H   projection center + screen height parameter
ctrl   29     ZSF3        AVSZ3 scale factor (1.19.12)
ctrl   31     FLAG        sticky error/saturation bits
```

## Fixed-point conventions

| Quantity      | Format          | Notes                          |
|---------------|-----------------|--------------------------------|
| vectors       | 1.3.12          | 4096 == 1.0                    |
| matrices      | 1.3.12          | two lanes packed per word      |
| TRX..TRZ      | 1.19.12         | full 32-bit                    |
| IR0..IR3      | int16 saturating| range selected by LM           |
| MAC0..MAC3    | wide            | internal sums up to 44 bits    |

## Wide intermediates vs architectural saturation

The multiplier produces up to 44 significant bits. The architectural MAC
register is 32 bits and the IR registers are 16 bits — so EVERY op must:

1. accumulate in wide math,
2. detect 44-bit overflow (FLAG bits 30/29),
3. shift right arithmetically when SF=1,
4. saturate on register write-back, honoring LM (unsigned clamp to
   0..32767 raising bit 27 vs signed clamp raising bit 28).

Keeping those stages separated in code is the single most important design
decision of a GTE core — mixing them is how you get wobbly geometry.

## FLAG register

Sticky within a command, recomputed per instruction. This chapter's
documented subset:

```
31 ERROR aggregate       30/29 MAC overflow pos/neg
28/27 IR saturation signed/unsigned(LM)
21/20 MAC0 overflow pos/neg
17 SF echo               16 LM echo
15..0 mirror of 31..16
```

## Implemented operations

- **RTPS (01h)** — rotate + project one vertex:
  `MACi = TR_i*10000h + Ri.v`, `IRi = sat16(MAC>>sf)`,
  `SZ3 = clamp(MAC3>>sf, 0..FFFF)`,
  divide `MAC0 = min(((H*20000h/SZ3)+1)/2, 20000h)` — SZ3==0 raises the
  divide-overflow flag and forces maxima,
  `SX2/SY2 = clamp(OFX/OFY + IR*MAC0>>16, -1024..1023)`.
- **RTPT (02h)** — RTPS over three vertices, pushing the SXY FIFO.
- **NCDS (05h)** — normal · light matrix · light-color matrix ·
  background accumulation into IR0 · far-color interpolation into RGB.
- **MVMVA (12h)** — general matrix-vector with selectable operands.
- **AVSZ3 (1Bh)** — `MAC0 = ZSF3*(SZ1+SZ2+SZ3)>>2` into IR0.
- **NCLIP (04h)** — coding test: 2D cross product for backface culling.

## Conformance testing

The challenge harness replays input-register snapshots through the ops and
compares FULL output state (MAC/IR/SZ/SXY/FLAG) against goldens generated
by the reference implementation. The public ps1-tests suite contains GTE
arithmetic tests runnable on real hardware dumps — referenced via
`requires_rom` and optional in the hidden manifest.
