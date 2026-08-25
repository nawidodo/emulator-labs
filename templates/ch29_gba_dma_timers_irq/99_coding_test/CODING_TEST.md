# CODING TEST — ch29: unseen trigger-mode spec (video-capture DMA)

You have never implemented this mode. Implement exactly the specification
below — the hidden grader runs a conformance suite against it.

## Specification: DMA3 capture trigger

When DMA3's start-timing bits equal `3` (the "special" value), the channel
enters **video capture** mode:

1. It fires once per scanline on lines **2 through 162 inclusive**. Line 2
   is the hardware prefill line; no extra transfer happens in this model.
2. The repeat control bit is irrelevant: firing is implied every qualifying
   line.
3. A count field of `0` decodes to the full `0x10000` units, as with other
   channels; otherwise the count is used verbatim each fire.
4. After servicing line 162 the channel finishes and its ENABLE bit reads
   as cleared.

Implement `capture_fires_on_line`, `capture_units` and
`capture_finished_after` in `capture.hpp`. Lines outside [2,162] must not
fire; boundaries are inclusive and exact.
