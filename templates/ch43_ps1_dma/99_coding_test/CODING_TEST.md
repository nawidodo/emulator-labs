# Coding test — ch43: repair linked-list chains (edge cases)

You are handed an unknown codebase function and a written specification.
Implement `ps1ct::inspect_chain` in `chain_inspect.hpp` **exactly to spec**;
hidden grading feeds your build chain files you have never seen.

## Chain format

Memory is a window of little-endian 32-bit words. A chain starts at byte
address `madr`. Every packet header word is:

```
[31:24] payload word count (0..255)
[23:0]  next link address in BYTES (must be 4-byte aligned)
```

## Specification

`inspect_chain(ram, madr, max_packets=256)` walks packets and returns:

- `Ok` — a packet's low-24-bit link equals exactly `0FFFFFFh`
  (the sentinel itself may sit in any valid header position).
- `SelfLoop` — a packet's link equals that packet's own header address.
- `PointerOutOfRange` — the starting address is unaligned or beyond the
  RAM window, OR any followed link is unaligned or beyond the window.
  Note: `0FFFFFFh` as a LINK is the terminator, never out-of-range.
- `MissingTerminator` — `max_packets` headers consumed without reaching
  the sentinel.

## Edge cases the hidden set exercises

1. Zero-length packets (`count == 0`) are legal; they still hop.
2. A single packet whose link is the sentinel → `Ok`.
3. Unaligned start address (`madr & 3 != 0`) → `PointerOutOfRange`
   before reading anything.
4. Link exactly one past the last valid word → `PointerOutOfRange`.
5. Two-node cycle without sentinel → `MissingTerminator` at the cap.

## Deliverable

Skeleton builds RED against the public tests below; solution passes all of
them AND the hidden manifest cases:

```bash
./ch43_ct_dma_tests                     # public unit tests
./ch43_ct_dma_tests tests/hidden/ch43_ps1_dma/ct/some.chain
# fixture mode: loads raw words, inspects from madr=0,
# prints "status=<Name>", exits 0 iff status==Ok else 1
```
