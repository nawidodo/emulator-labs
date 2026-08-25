# 01 — The CPU Bus: Mirrors, Registers, Pads, DMA

The 6502 puts an address on the wire; the bus decides who answers. Every
region has its own decode rule — and its own mirroring trap.

## Tasks

1. **RAM window** — $0000-$1FFF answers a 2KB chip repeated eight times
   (`addr & $07FF`). Reads and writes share the rule.
2. **PPU window** — $2000-$3FFF decodes to EIGHT registers, mirrored every
   8 bytes (`addr & $0007`); `$3FFF` is `$2007`.
3. **Controllers** — $4016/$4017: strobe high keeps reloading the shift
   register from live button state; the falling edge freezes it; reads
   then walk A..Right LSB-first and feed 1s after bit 8.
4. **OAM DMA** — writing $4014 copies 256 bytes of one CPU page into
   primary OAM and debits exactly 513 CPU cycles (+1 when the copy
   begins on an odd cycle count).

## Acceptance

- All `mirrors.*`, `ppudec.*`, `controllers.*`, `dma.*` tests pass,
  including exact DMA cycle debits (514/515 including the trigger write).
