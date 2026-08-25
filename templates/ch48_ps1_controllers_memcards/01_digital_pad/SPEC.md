# ch48 ex01 — Digital pad — SPEC

This file pins the exact pad model used by the tests and goldens. Where real
hardware allows variation, THIS course's behavior is what is graded.

## Framing

* `/SELECT` low opens a session; the rising edge wipes any partial state.
* First transmitted byte of a session = device select: `0x01` pad,
  `0x81` memory card. The bus (ex03) routes it by CTRL bits 12/13.
* Every byte is clocked through `1F801040`; each byte advances the serial bit
  counter by 8.

## Pad read transaction

```text
tx: 01 | 42 | 00 | 00 | 00 | 00
rx: FF | FF | 41 | 5A | b.lo | b.hi
```

* Response to the select byte and to the command byte itself is `0xFF`.
* ID word is `0x5A41`, transmitted LOW byte first (`41`, then `5A`).
* Button halfword follows, low byte first, ACTIVE-LOW
  (bit clear = pressed). Layout: bit0 select, 1 l3, 2 r3, 3 start, 4 up,
  5 right, 6 down, 7 left, 8 l2, 9 r2, 10 l1, 11 r1, 12 triangle, 13 circle,
  14 cross, 15 square. L3/R3 always read released on a digital pad.
* Unknown commands: `0xFF` until deselect; the session recovers afterwards.
* Overrun clocks after buttons-hi return `0xFF`.

## ACK model

ACK is asserted after every response byte that is NOT the final one of the
transaction — i.e. after `41`, `5A` and `b.lo`; never after `b.hi`, never
outside an armed session. `ack()` is combinational (true between bytes).
The bus maps it to STAT bit 7 NON-inverted: `STAT.7 == 1` while ACK asserted.
