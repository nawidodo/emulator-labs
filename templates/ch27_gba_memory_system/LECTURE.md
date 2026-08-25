# Chapter 27 — GBA Memory System

The GBA's 32-bit bus routes every CPU access to one of nine regions, each
with its own bus width, wait-state behavior and mirroring rules. This
chapter builds that routing layer: region decode, access widths with ARM
rotation semantics, wait-state cycle accounting, and the `BusResult`
interface everything downstream consumes.

## The memory map

| Region      | Range                 | Size  | Bus | Notes                          |
|-------------|-----------------------|-------|-----|--------------------------------|
| BIOS        | 0x00000000–0x00003FFF | 16 KB | 32  | only readable in privileged modes (modeled: always readable) |
| EWRAM       | 0x02000000–0x0203FFFF | 256 KB| 16  | mirrored every 0x20000 through 0x02FFFFFF |
| IWRAM       | 0x03000000–0x03007FFF | 32 KB | 32  | mirrored every 0x8000 through 0x03FFFFFF |
| IO          | 0x04000000–0x040003FE | 1 KB  | 32  | register file                  |
| Palette     | 0x05000000–0x050003FF | 1 KB  | 16  |                                |
| VRAM        | 0x06000000–0x06017FFF | 96 KB | 16  | see mirror rule below          |
| OAM         | 0x07000000–0x070003FF | 1 KB  | 32  |                                |
| ROM (WS0/1/2)| 0x08000000–0x09FFFFFF etc. | up to 32 MB | 16 | three waitstate chips |
| SRAM        | 0x0E000000–0x0E007FFF | 32 KB | 8   | 8-bit bus only                 |

## VRAM mirror discontinuity

VRAM is 96 KB: 64 KB of background memory followed by 32 KB of object
memory. Two rules pin the routing:

- **128 K boundary**: the whole window mirrors — an address is folded by
  `addr & 0x1FFFF` before anything else.
- **OBJ discontinuity**: the object half (`offsets A000–AFFF` of each
  bank) does *not* participate in a 64 K intra-mirror. A naive
  `addr & 0xFFFF` fold would alias 0x06014444 onto 0x06004444 — hardware
  does not. The unmapped hole 0x06018000–0x0601FFFF instead reflects the
  BG upper half: canonical = `0x8000 + (offset & 0x7FFF)`.

```text
offset = addr & 0x1FFFF            // 128 K mirror window
offset >= 0x18000 -> offset -= 0x10000   // top hole -> BG upper half
else              -> direct (BG or OBJ bank, no further folding)
```
The bus honors 8/16/32-bit requests. On a **non-aligned word read**
(ADDR % 4 != 0) hardware does not fault: the aligned word is fetched and
**rotated right by (addr & 3) * 8** so the requested byte lands in the low
position. Halfword accesses behave analogously on 16-bit buses. Writes are
simply masked/truncated to the target width.

## Wait states

Every region has a non-sequential (N) and sequential (S) cost. An access
is S when it directly follows the previous access of the same size to the
adjacent address in the same region — otherwise N. After a branch (or any
pipeline refill) the next access is always N: prefetch is **off** in this
baseline model, so nothing hides the refill.

| Region  | N | S |
|---------|---|---|
| BIOS/IWRAM/IO/Palette/VRAM/OAM | 1 | 1 |
| EWRAM   | 3 | 2 |
| ROM WS0 | 4 | 2 |
| ROM WS1 | 3 | 2 |
| ROM WS2 | 5 | 2 |
| SRAM    | 5 | 2 |

## Open bus

Reading unmapped or write-only space returns the **last value driven onto
the data bus** (simplified policy): the previous successful read value,
zero-initialized at reset.

## The BusResult interface

Every bus operation returns both payload and cost:

```cpp
struct BusResult {
    uint32_t value;
    unsigned cycles;
};
```

Downstream systems (DMA, CPU fetch, PPU) consume `BusResult` instead of
poking raw arrays — timing stays honest end to end.
