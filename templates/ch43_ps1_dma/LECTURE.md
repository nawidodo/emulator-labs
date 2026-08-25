# Chapter 43 — PS1 DMA Controller

The PlayStation's CPU never blits a framebuffer, never fills the MDEC
input FIFO and never streams CD sectors by hand. Seven dedicated DMA
channels move the data while the CPU runs game logic. This chapter builds
the controller register model, both block transfer modes with chopping,
and the two linked-list engines that make the PS1 special: the OTC
ordering-table builder and the GPU packet-list walker.

## Register map

```
1F801080h+n*10h+0  MADR  transfer base address
1F801080h+n*10h+4  BCR   [31:16] block count   [15:0] words per block
1F801080h+n*10h+8  CHCR  channel control
1F8010F0h          DPCR  control for all seven channels
1F8010F4h          DICR  interrupt enable / flags
```

Channels are fixed by hardware:

| # | Name    | Typical use                          |
|---|---------|--------------------------------------|
| 0 | MDECin  | RAM -> MDEC compressed macroblocks   |
| 1 | MDECout | MDEC -> RAM decoded image data       |
| 2 | GPU     | command lists / VRAM transfers       |
| 3 | CDROM   | sector data FIFO drain               |
| 4 | SPU     | audio sample upload                  |
| 5 | PIO     | expansion port (rarely used)         |
| 6 | OTC     | ordering-table builder               |

## CHCR — channel control

```
bit 0      direction: 0 = RAM -> device, 1 = device -> RAM
bits 8-9   sync mode: 0 burst, 1 slice, 2 linked list
bits 16-18 DMA chopping window (encoded, see below)
bits 20-22 CPU chopping window (encoded, see below)
bit 24     start / busy
bit 28     force trigger (start without software trigger bit)
```

## Block modes: burst vs slice

- **Burst** moves `BCR.blocks * BCR.words` words in one uninterrupted
  run. Simple, but starves the CPU for the whole duration.
- **Slice** moves chunks of the DMA-chopping window, then yields the bus
  for the CPU-chopping window before the next chunk. Games use this to
  keep interrupt latency low during large SPU uploads.

Chopping fields encode `(n+1)` units; our documented decode is
`dma_window_words = (n+1)*4` and `cpu_window_cycles = (n+1)*8`. A zero
field disables chopping on that side. The cycle accounting used by all
goldens in this chapter is deliberately simple and deterministic:
one cycle per moved word, plus one CPU window between chunks.

## Linked-list mode

Sync mode 2 ignores BCR entirely. Each packet header word is:

```
[31:24] payload word count (0..255)
[23:0]  next packet address in bytes
```

Two rules cause most real bugs:

1. **Termination-on-last-pointer-exact**: only a link of exactly
   `0FFFFFFh` ends the chain. Zero is NOT a terminator — it hops to
   address 0. A walker that stops on zero walks straight into stale RAM.
2. **Headers never reach the device.** The DMA unit consumes each header;
   the GPU receives exactly `[31:24]` payload words. Off-by-one here
   shifts every subsequent command.

### OTC (channel 6)

The ordering-table builder constructs backwards tables used by the GPU
OTC primitive-sorting idiom: entry at `start` points to `start-4`, and so
on; the final entry is the exact sentinel `0FFFFFFh`. Games allocate an N-
entry table, let OTC wire it up in one DMA burst, then insert sprite/draw
packets at each node under CPU control and hand the whole table to the GPU
list walker.

## DPCR and DICR

DPCR packs one nibble per channel: bit 3 = enable, bits 2:0 = priority
(7 highest). A channel runs only when its DPCR enable AND its CHCR start
(or force-trigger) bit are set.

DICR (our documented subset):

```
bits 0-5    force IRQ per channel (software)
bits 16-21  IRQ enable per channel
bits 24-29  completion flags, cleared by writing ONES
bit 30      master enable
```

IRQ raises when the master enable is on and any channel has
`(completion_flag | force) & irq_enable`.

## Endpoints

Every device hangs off a two-method interface (`write_word` /
`read_word`). That keeps the DMA engine independently testable — you can
drive it from a vector-backed fake device exactly like curriculum §57
demands — and lets later chapters plug the real GPU/CDROM/SPU in.

## Debugging workflow

Trace first (curriculum §54). Dump the walk trace (`pkt= ptr= hdr= words=
cyc=`) and compare against the expected capture word-by-word; the FIRST
divergent word tells you which header was misparsed or which link was
misread. The debug exercise seeds exactly such bugs.
