# Chapter 48 Lecture — Controllers, Memory Cards and Serial I/O

The PS1 talks to its pads and memory cards over a slow serial bus: the SIO
register window at `1F801040..1F80104E`, two physical controller/memcard
slots, and a protocol so simple you can replay it by hand. This chapter
builds the whole stack: pad state machine, card state machine, card images,
and a dual-slot bus a headless runner can script.

## Study list

```text
SIO registers
device select framing
digital pad protocol
ACK handshake
memory-card commands
XOR checksums
bad-sector flags
.mcr card images
dual slots
```

## Registers

| Address    | Name | What we model                                             |
|------------|------|-----------------------------------------------------------|
| `1F801040` | TXRX | write = transmit one byte, read = pop the response byte   |
| `1F801044` | STAT | bit 0/1 TX-ready (always 1 here); bit 7 ACK input level   |
| `1F801048` | MODE | stored verbatim; no timing effect in our model            |
| `1F80104A` | CTRL | bit0 TXEN, bit2 ACK-IRQ-en, bit5 reset devices, bits12-13 slot select |
| `1F80104E` | BAUD | stored; each byte moves the serial bit counter by 8       |

CTRL bits 12 (`SLT1`) and 13 (`SLT2`) decide which slot answers. If both are
set slot 2 wins; if neither is set the bus reads `0xFF`. Our STAT bit 7 is a
**non-inverted** ACK level — real hardware inverts it; we chose the readable
variant and pin it with tests so there is no ambiguity.

## Device-select framing

Every session starts when the host pulls `/SELECT` low and transmits one
byte that names the device KIND:

* `0x01` → the pad of the selected slot answers;
* `0x81` → the memory card of the selected slot answers.

Each slot carries BOTH devices independently: two pads + two cards behind
the two CTRL bits. Raising `/SELECT` resets whatever half-finished state the
device was in. One arbitration rule keeps scripted logs unambiguous: a
select byte (`0x01`/`0x81`) is honored only while no transaction is in
flight; mid-transaction those values are just payload bytes.

## Digital pad

After arming on command `0x42`, the digital pad returns four bytes:

```text
tx: 01 | 42 | 00 | 00 | 00 | 00
rx: FF | FF | 41 | 5A | b.lo | b.hi
```

The ID word is `0x5A41`, low byte first. The button halfword is active-low
(bit clear = pressed) with the layout documented in `01_digital_pad/pad.hpp`
— bit 0 SELECT through bit 15 SQUARE. A digital pad never presses L3/R3.
Unknown commands answer `0xFF` until deselect but do not brick the device.

## ACK

A device asserts ACK after every response byte that still has a successor,
and drops it once the final byte has been clocked out. For the pad read:
after `41`, after `5A`, after `b.lo` — not after `b.hi`. The card behaves
the same way across its longer transactions. In this course ACK is a
combinational query (`ack()`), which keeps tests deterministic without
simulating the baud-rate timer.

## Memory-card protocol

Card commands, each followed by three address bytes MSB-first:

| Cmd  | Name  | Effect                                            |
|------|-------|---------------------------------------------------|
| 0x52 | READ  | stream 128 payload bytes + XOR checksum           |
| 0x57 | WRITE | accept 128 payload bytes + checksum, then commit  |
| 0x53 | GETID | return the fixed ID `04 00 00 80`, checksum `84`  |
| 0x43 | ERASE | fill the sector with `0xFF`                       |

Flag bytes tell you where you are in the frame: `0x5D` pre (response to the
command byte), `0x5C` mid (address phase, and every write data byte),
`0x5A` good end. Our addressing model takes `sector = ((a0<<8)|a1) & 0x3FF`
and ignores `a2` — ten bits cover all 1024 sectors exactly.

The checksum is an XOR over exactly the payload bytes. Never over flags or
addresses; GETID's checksum is `04^00^00^80 = 0x84`.

## Bad sectors

Real cards keep fault maps in vendor areas. We need something deterministic
to grade, so OUR model puts a bad flag in the directory: attribute byte 21
of an entry, bit 7 set, marks ALL sectors of that block bad. Reads of a bad
sector answer payload `0xFF` × 128 with checksum `0x00` (which is just the
XOR of 128 ones) and end with `0xFF` instead of `0x5A`; writes to it are
discarded and also end `0xFF`.

## Card images

A standard headerless `.mcr` file is 131072 bytes: 1024 sectors × 128. When
the curriculum speaks of "blocks" it means the file's natural 8 KiB units —
16 blocks × 64 sectors each. Sector 0 is the header (we write `"MC"` at
bytes 0–1), sectors 1..15 are directory entries (entry *i* ↔ block *i*),
and the rest is data. See `03_card_image/card_image.hpp` for the exact entry
layout this course grades against.

## Dual slots

Two slots, both kinds of device per slot, CTRL bits 12/13 to pick. That is
the whole story — and exactly what the runner scripts exercise when they mix
pad reads and card transactions in one log.
