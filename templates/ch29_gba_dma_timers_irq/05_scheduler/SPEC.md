# 05_scheduler — SPEC

Build the guest-cycle event scheduler and wire the system.

1. `Scheduler::schedule` / `run_until` — earliest-effective-time first;
   identical timestamps resolve by insertion order (determinism!).
2. DMA preemption — `begin_dma_burst` makes the bus busy; events inside the
   window fire when the burst ends.
3. `next_overflow_in` — cycles until a live counter overflows.
4. `next_video_event` — HBlank at +960 per line, line starts, VBlank opens
   entering line 160.

The provided `HWSystem` glues bus/DMA/timers/IRQ to the scheduler; the
runner executes `.hws` scripts headless with trace + state-hash output.
