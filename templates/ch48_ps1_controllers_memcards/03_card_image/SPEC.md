# ch48 ex03 — Card images, dual slots and the SIO runner — SPEC

## .mcr image layout

131072 bytes = 1024 sectors × 128 bytes. The curriculum's "blocks" are the
file's natural 8 KiB units: 16 blocks × 64 sectors. Mapping:

* sector 0 — header: bytes 0..1 = `"MC"`, everything else `0xFF`;
* sectors 1..15 — directory entries, entry *i* ↔ data block *i*;
* block *b* (1..15) — data at sectors `[64b, 64b+63)`.

Directory entry (128 bytes): byte 0 state (`0xA0` in use, `0x51` deleted,
`0xFF` free), bytes 2–3 size in blocks LE, bytes 4–19 ASCII name zero-padded,
byte 21 attribute — **bit 7 set = whole block flagged bad** (course model;
real cards keep vendor fault maps, we need deterministic testable behavior).
All other bytes `0xFF`. Mounting an image rescans the directory and flags
every sector of a bad block.

## Dual-slot bus

CTRL (`1F80104A`) bits: 0 TXEN, 2 ACK-IRQ-enable, 5 reset devices,
12 slot 1 select, 13 slot 2 select (both set → slot 2 wins; none → bus
reads `0xFF`). STAT (`1F801044`): bits 0–1 TX-ready always 1; bit 7 = ACK
level NON-inverted (1 while the armed device holds ACK). MODE/BAUD stored;
each transferred byte advances the serial bit counter by 8.

Device-select framing: `0x01` picks the pad of the selected SLOT, `0x81`
its card. A select byte takes effect only when NO transaction is in flight
on that slot; while one IS in flight it is ordinary payload data (pads and
cards both emit `0x01`/`0x81` as button/payload bytes). When idle, a select
byte re-arms (or switches) the armed device and draws `0xFF` itself.

## Runner grammar (`ch48_03_sio_runner`)

CLI: `--rom PATH --headless --cycles N --frames N --trace FILE --hash-frame
FILE --input-file FILE`. The ROM is a `.mcr` mounted into slot 1's card.
`--hash-frame` receives the concatenated response bytes of every XFER and
its FNV-1a-64 digest is printed as `fnv64 <16 hex>`.

Script commands (one per line; `#` comments; blank lines ignored):

```
SLOT <0|1>            active slot -> CTRL bits 12/13
PAD <word-hex>        active-low button halfword into the slot's pad
XFER <hex> [<hex>...] clock bytes through 1F801040; responses are hashed
CARDFILE <path>       mount .mcr into active slot's card (rescans bad flags)
CARDSTORE <path>      export active slot's card image to path
RUN                   no-op commit marker
```

Trace lines (canonical shape, one per XFER byte):

```
pc=1f801040 op=<tx> reg=rx=<rx> reg=slot=<n> cyc=<serial bits>
```

Paths in scripts resolve relative to the working directory of the runner
(the repo root under grading).
