# 04_eeprom_dmac — SPEC

Implement the EEPROM serial protocol over DMA3 halfword streams
(bit 15 of each u16 = one bit, MSB-first).

1. `feed` — bit machine: start 1, op "10"=read / "00"=write, address
   (6 bits @512 B, 14 @8 KB), then 64 data bits (write, MSB-first) or a
   queued 64-bit answer (read). `stop()` returns to Idle.
2. `queue_read_response` / `read_bit` — serve the addressed 64-bit word.
3. `feed_dma_stream` — split u16s into 16 fed bits each.

Acceptance: write/read roundtrip on both sizes, idle tolerance of zero bits.
