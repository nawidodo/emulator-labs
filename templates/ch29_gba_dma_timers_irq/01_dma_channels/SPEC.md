# 01_dma_channels — SPEC

Implement GBA DMA immediate transfers.

1. `addr_step` — signed byte step per unit for the four address-control
   values (inc, inc+reload acts as inc, dec, fixed), scaled by width.
2. `run_immediate_transfer` — copy `effective_count()` units (count 0 =
   full 0x10000 range) from SAD to DAD honoring 16/32-bit width; charge
   6 cycles/halfword, 8/word; raise completion IRQ when configured; clear
   ENABLE afterwards.

Acceptance: forward copies, fixed-source fills, decrementing destinations,
word path and cycle accounting all exact.
