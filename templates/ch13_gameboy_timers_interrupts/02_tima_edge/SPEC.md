# 13.02 — TIMA on the falling edge

Wire the timer unit onto the divider: the TAC gate ($FF07 bit 2) and the
falling-edge detector that clocks TIMA ($FF05).

## Contract

* Exact bit-select table (TAC bits 1-0 -> tapped DIV counter bit):
  `00->9, 01->3, 10->5, 11->7` — periods 1024/16/64/256 T-cycles.
* The tapped bit is sampled once per 4-T-cycle block whether gated or not;
  while gated, a sampled 1->0 transition increments TIMA.
* Enabling TAC mid-stream is deterministic: if the tapped bit already
  reads 1, its next natural fall still counts.
* Disabling TAC (bit 2 1->0) while the tapped bit reads 0 AFTER the write
  produces exactly one increment (the "disable edge").
* A select change that makes the sample stream appear to fall also counts,
  matching hardware's mux glitch.
* On the $FF->$00 wrap TIMA wraps to $00 and sets `overflow_pulse`; reload
  policy is exercise 03's business.

## Target

`ch13_02_edge_tests`
