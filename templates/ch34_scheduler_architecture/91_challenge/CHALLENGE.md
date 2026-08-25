# Challenge — convert a tick-driven timer to event scheduling (ch34)

The ch13 timer is a classic **tick-driven down-counter**: the machine calls
`tick()` once per master cycle and the device counts down internally. It
works, but it burns a branch and a decrement on every single cycle for a
device that fires once every few thousand cycles.

Your task: replace the per-cycle ticking with scheduler events while
producing a **bit-exact** flag timeline.

## The legacy interface (`legacy_timer.hpp`, do not modify)

```cpp
LegacyTimer t;
t.set_period(6);   // takes effect on the NEXT reload, mid-count writes pending
t.tick();          // one master cycle: counter--, reload+flag at underflow
t.flag();          // raised at each underflow
t.clear_flag();
```

Exact semantics (ch13 spec):

- `set_period(p)` stores p; the counter reloads from it only at underflow.
- `tick()`: if counter > 0, decrement. When the counter reaches 0 by this
  decrement, raise the flag and load the counter with the *stored* period.
- A period of 0 means "timer stopped": `tick()` does nothing.

## Your job

In `timer_events.hpp`, implement `EventTimer`, an adapter that:

1. never gets ticked — instead it answers `next_event()` (absolute deadline
   of its next underflow) and applies `fire(now)` when dispatched;
2. reproduces the legacy flag timeline exactly, including the awkward
   corner cases:
   - writing a new period while counting (takes effect at next reload),
   - being serviced late (catch-up): several underflows may be due; the
     adapter must apply them all, in order, before reporting the next one,
   - stop (period 0) and restart.

The equivalence harness in `main.cpp` runs both implementations against an
identical scripted sequence of period writes and cycle advances and
requires identical flag timelines. Acceptance: every `equivalence` test
passes with your `EventTimer`.

## Why bit-exact matters

If a converted timer drifts even one cycle, games that sync audio or DMA
to timer IRQs break in ways that take weeks to trace back here.
