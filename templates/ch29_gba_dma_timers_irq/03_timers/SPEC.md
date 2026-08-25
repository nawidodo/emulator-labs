# 03_timers — SPEC

Implement GBA timers with exact reload semantics.

1. `prescale_cycles` — field 0..3 -> 1/64/256/1024 cycles per tick.
2. `timer_advance_ticks` / `timer_tick` — closed-form up-counting; wrap
   0xFFFF -> reload exactly (period = 0x10000 - reload); sub-tick leftover
   cycles reported so callers keep phase.
3. `chain_tick` — cascade: previous timer's overflow count is the next
   timer's tick budget when the cascade bit is set; IRQ flags per overflow.

Acceptance: periods are exact across split calls (no lost partial ticks);
a period-1 timer cascaded into a period-2 timer divides by 2.
