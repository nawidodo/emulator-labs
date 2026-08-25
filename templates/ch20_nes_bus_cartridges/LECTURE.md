# Chapter 20 — NES Bus and Cartridges

The 6502 in an NES never talks to memory directly — every address goes
through a bus whose decode logic decides who answers, and above $4017 the
answer comes from the cartridge you snapped in. Understanding this layer is
what turns "a CPU that runs" into "a machine that boots".

## The CPU's address map

```text
$0000-$1FFF   2KB internal RAM, MIRRORED every $0800
              (only A10-A0 reach the chip)
$2000-$3FFF   PPU registers — EIGHT of them, mirrored every 8 bytes
$4000-$4013   APU
$4014         OAM DMA trigger (one write = one sprite-table page copy)
$4015         APU status
$4016/$4017   controller ports 1/2
$6000-$7FFF   cartridge PRG-RAM (later chapters)
$8000-$FFFF   cartridge PRG-ROM via the Mapper
```

Two mirroring rules cause most first-draft bugs:

1. **RAM**: `addr & $07FF`. A pointer bug at `$0FFF` corrupts `$07FF` and
   nothing else — mirrors mean corruption is *structured*.
2. **PPU registers**: `addr & $0007`. `$3008` IS `$2000`. Games rely on it.

## Controllers: a shift register wearing a port

Writing $4016 bit0 high reloads the shift register from live button state;
the 1->0 edge freezes a snapshot. Each read then emits one bit LSB-first —
A, B, Select, Start, Up, Down, Left, Right — feeding 1s in afterwards,
which is why polling loops read exactly eight times.

## OAM DMA: the cycle bill is part of the API

One write to $4014 copies 256 bytes of any CPU page into sprite memory.
The cost is not implementation detail: 513 cycles, +1 when started on an
odd cycle (get/put alignment). Games time mid-frame effects against this
debit; get it wrong and sprite glitches follow.

## iNES and the Mapper connector

The iNES header is the cartridge's business card: bank counts, mirroring
arrangement, mapper number. The bus never knows what mapper is plugged
in — it calls `cpu_read`/`cpu_write` on a `Mapper*`. NROM (mapper 0) is
the hello-world: one or two PRG banks, CHR on the PPU side, hard-wired
nametable mirroring. Its quirk: a single 16KB bank appears TWICE so the
reset vector at $FFFC always exists.

## Study references

- https://www.nesdev.org/wiki/CPU_memory_map
- https://www.nesdev.org/wiki/Standard_controller
- https://www.nesdev.org/wiki/OAM_DMA
- https://www.nesdev.org/wiki/INES
- https://www.nesdev.org/wiki/Mirroring
- https://www.nesdev.org/wiki/NROM
