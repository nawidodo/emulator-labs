# 02_dma_triggers — SPEC

Implement triggered DMA.

1. `dma_should_fire` — timing match, enable check, once-per-enable without
   repeat.
2. `arbitrate` — fixed priority DMA0>DMA1>DMA2>DMA3 for same-instant
   requests.
3. `fifo_refill` — exactly four words per burst, 8 cycles/word, DAD reloaded
   to base afterwards.
4. `run_video_event` — dispatch HBlank/VBlank channels in arbitration order
   at a given guest cycle; report actions.

Video constants: 1232 cycles/line, HBlank at cycle 960 of the line,
VBlank from line 160.
