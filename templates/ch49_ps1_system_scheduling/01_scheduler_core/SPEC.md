# 01 — Scheduler core

One master clock, one event queue. Every device in this chapter advances
ONLY through events dispatched by `sched::Scheduler` — there is no
fixed-cycle polling anywhere.

## Contract

```cpp
uint64_t schedule(uint64_t ts, Handler fn, const char* name); // returns id
void     cancel(uint64_t id);      // lazy: marked, discarded at dispatch
bool     step();                   // dispatch exactly the earliest event
void     run_until(uint64_t dl);   // dispatch while earliest.ts <= dl
```

Guarantees under test:

1. **Order** — earliest absolute timestamp first.
2. **Deterministic ties** — equal timestamps dispatch FIFO by insertion
   sequence number. This is what makes interrupt latch order reproducible
   when the CD completion and an SPU sample boundary land on the same
   master-clock cycle.
3. **Reentrancy** — a handler may schedule new events, even at its own
   timestamp; they queue behind it in insertion order.
4. **Deadline semantics** — `run_until(d)` is inclusive at `d`; events
   scheduled during dispatch with `ts <= d` fire before it returns.
5. **Cancel** — a cancelled event never fires and never disturbs heap
   order of others.
