# ch48 ex02 — Memory-card protocol — SPEC

Pins the exact card model graded by tests and goldens.

## Geometry

* 128-byte sectors, 1024 sectors = 131072 bytes (the standard headerless
  `.mcr` file). The curriculum's "blocks" = 64-sector / 8 KiB units; a card
  holds 16 of them. Sector addressing is flat 0..1023.

## Transactions

The bus consumes the `0x81` device-select byte first; the frames below show
what the CARD sees (bus-level frames in 03_card_image/SPEC.md).

```text
READ   tx: 52 | a0 | a1 | a2 | 00 * 130
       rx: 5D | 5C | 5C | 5C | d0..d127 | chk | 5A
WRITE  tx: 57 | a0 | a1 | a2 | d0..d127 | chk | 00
       rx: 5D | 5C | 5C | 5C | 5D * 129       | 5A
GETID  tx: 53 | 00 | 00 | 00 | 00 * 6
       rx: 5D | 5C | 5C | 5C | 04 00 00 80 | 84 | 5A
ERASE  tx: 43 | a0 | a1 | a2 | 00
       rx: 5D | 5C | 5C | 5C | 5A
```

Flags: `0x5D` pre (response to the command byte), `0x5C` mid (address phase;
write data phase), `0x5A` good end. Unknown commands answer `0xFF` until
deselect and the session recovers afterwards.

## Addressing

Three bytes MSB-first; `sector = ((a0 << 8) | a1) & 0x3FF`; `a2` is clocked
but ignored. Out-of-range addresses wrap through the mask.

## Checksum and bad sectors

* `chk` = XOR over exactly the 128 payload bytes (GETID: over its 4 ID bytes
  = `0x84`). Never over flag/address bytes.
* A sector flagged bad in the directory answers all-`0xFF` payload with
  checksum `0x00` (= XOR of 128 × FF) and ends `0xFF`. Writes to it are
  discarded and also end `0xFF`.
* A write commits only if `chk` matches; a mismatching checksum changes NO
  byte and ends `0xFF`.
* ERASE fills the target sector with `0xFF`.

## ACK

Asserted after every non-final response inside an armed session — including
right after the PRE flag — dropping only once the final flag has been sent.
