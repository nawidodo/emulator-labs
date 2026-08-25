# 99 — Coding Test: The Mapper-Zero CHR-RAM Variant

Supplied spec (no peeking at other emulators). Mapper 0 cartridges exist in
two flavors distinguished ONLY by the header's CHR bank count:

| Header CHR banks | CHR hardware | PPU $0000-$1FFF behavior |
|---|---|---|
| 0 | 8KB CHR-**RAM** (board-wired /WR) | reads AND writes; 8KB window |
| > 0 | 8KB-per-bank CHR **ROM** | reads only; writes silently dropped |

Nametable traffic ($2000+) is unaffected: CIRAM through
`mirror_translate()`, both directions, per the header's mirroring bit.

Implement `ChrRamNROM::ppu_read` / `ppu_write` against that table. The
`chr_is_ram` flag is already derived for you by `create()`.

## Acceptance

- All `unseen.*` tests pass: round trips, the $2000 wrap, ROM write drops,
  and CIRAM unaffected.
