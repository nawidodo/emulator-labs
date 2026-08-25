# Coding Test — ch14: unseen snapshot extension (v2 "BGP boost")

Implement the v2 snapshot trailer from THIS specification alone. You have
the chapter-14 renderer as a starting point; nothing here is covered by the
exercises.

## Specification

A PPU state image is **v1** (8198 bytes, exercises 01-05) or **v2**
(8202 bytes). A v2 image is a v1 image followed by a 4-byte trailer:

| offset | size | content |
|---|---|---|
| 0x2006 | 2 | magic bytes `'B'`, `'X'` (0x42, 0x58) |
| 0x2008 | 1 | boost amount `b`; **only bits 1:0 are meaningful** (mask 0x03) |
| 0x2009 | 1 | flags; **bit 0** = boost enabled |

Semantics when loading:

* exactly-kSnapshotSize file -> v1, behave identically to exercise 04.
* exactly-kSnapshotSizeV2 file whose trailer magic matches -> latch
  `boost_amt = byte[0x2008] & 3` and `boost_en = byte[0x2009] & 1`.
* any other size, or wrong magic on a v2-size file -> load failure.
* boost disabled (`bit0 == 0`) -> rendering must be bit-identical to v1,
  regardless of the amount byte.

Rendering semantics when enabled: every BG shade index `s` (after BGP
mapping) becomes `min(3, s + b)` before the grayscale ramp. Boost therefore
only ever darkens or preserves; shade 3 is unaffected by clamping and shade
`0` maps to `b`.

## Deliverables

* `ppu.hpp`: loader accepts both versions; renderer applies the boost.
* Visible tests in `main.cpp` cover passthrough, darkening, masking, clamp.

The hidden grader renders committed v2 snapshots through your runner and
compares full-frame hashes. Anything that brightens a single pixel fails.
