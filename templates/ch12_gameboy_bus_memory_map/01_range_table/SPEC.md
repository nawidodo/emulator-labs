# SPEC — 01_range_table

Implement the routing core of `gbmap::Bus` in `bus.hpp` (four `TODO`
blocks):

1. `findRange(addr)` — linear scan of the ordered table; the FIRST entry
   with `lo <= addr <= hi` wins. Ranges are inclusive on both ends.
2. `Bus::read` — route hits to `entry->device->read(addr)`, misses to
   `unmappedRead`.
3. `Bus::write` — same, with `unmappedWrite`.
4. Gap policy — unpopulated slots read `$00` (`kOpenBusByte`) and drop
   writes. This is a documented chapter policy; do not "improve" it.

Supporting cast is provided complete: `Device` (the virtual every region
implements), `Ram` (sized to its range, wrap-free indexing), ordered
`attach`, overlay primitives `attachFront`/`detach`.

Acceptance: `ch12_01_range_tests` green — every gap/boundary probed,
devices provably isolated, overlays resolved by table order.
