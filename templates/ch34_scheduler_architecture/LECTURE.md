# Lecture 34 — Scheduler Architecture

So far every machine you built advances time the same way: `for (i = 0; i <
cycles; ++i) tick();`. Every component is touched on *every* cycle, whether
or not anything interesting happens to it. Real hardware is not like that.
A GB timer fires once every 1024 T-cycles; a PS1 CD-ROM drive delivers a
sector after tens of thousands of cycles. Touching them every cycle is
wasted work — and worse, it forces you to invent per-device counters that
drift out of sync with the master clock.

This chapter replaces blanket ticking with an **event queue**.

## The five models of time, compared

```text
instruction stepping   run one CPU instruction; devices never advance
                       (fine for CHIP-8, wrong for anything with timing)

cycle stepping         advance everything one cycle at a time
                       + trivially correct ordering
                       - O(cycles) work even when nothing changes
                       - tempting to sneak in host-time hacks

event queue            each device publishes its NEXT interesting moment;
                       the scheduler jumps straight to it
                       + O(events) instead of O(cycles)
                       + ordering between devices is explicit
                       - you must handle ties and catch-up yourself

catch-up model         a device that was last serviced at cycle A, now
                       serviced at cycle B > A, first processes all the
                       deadlines it missed in [A, B] — in order

master vs local clocks ONE integer master clock (guest cycles since power-
                       on). Devices keep *derived* local state (last-fire
                       cycle, accumulator) but never their own idea of
                       "now".
```

## The one rule that matters

**Time is an integer guest clock: `uint64_t` guest cycles since reset.**

- Never call `std::chrono`, never read the wall clock, never seed anything
  from host time. Host time makes traces unreproducible and golden tests
  impossible.
- Fractions of a cycle (a device at 1.5x the CPU rate) are handled by
  *integer accumulators*: `acc += ratio_num; if (acc >= ratio_den) { acc -=
  ratio_den; fire(); }` — the same trick as Bresenham.
- Every scheduled timestamp is an absolute value on the master clock.

Real machines do this too: desmume, mGBA, and dolphin all schedule on
integer guest ticks (dolphin's DSP/VI/AI events live on a `CoreTiming`
event queue keyed to CPU cycles).

## The scheduler contract

```cpp
schedule(timestamp, event);   // absolute guest-cycle deadline
step();                       // dispatch exactly the earliest event
run_until(limit);             // dispatch while next.timestamp <= limit
```

Three properties your implementation must guarantee:

1. **Order**: earliest timestamp first.
2. **Deterministic ties**: two events at the same timestamp dispatch in
   insertion order (FIFO). Hardware does not reorder its own interrupts;
   neither may you. Implement it with a monotonic sequence number, never
   with pointer identity or heap internals.
3. **Reentrancy**: an event handler may schedule new events, including at
   its own timestamp. The freshly scheduled event must not be dispatched
   until its turn comes in queue order.

## Catch-up, concretely

A timer programmed for period 10 fires at cycles 10, 20, 30... Suppose the
CPU stalls it (interrupt disabled) from cycle 12 to 35. When re-enabled,
the hardware does not wait a full period; the counter resumes where the
real silicon would be. In event terms: the device's `fire()` computes what
*should* have happened during [12, 35] — typically "reload happened at 20
and 30" — and raises the flag accordingly, then reschedules relative to
the **master clock**, not relative to when someone remembered to ask.

The classic bug is scheduling `next = now + period` inside a late handler.
That silently stretches time. Reschedule as `next = last_deadline +
period` (looping forward while `next <= now` if several periods elapsed).

## Master clock vs device-local clocks

One master clock owns "now". A device keeps:

```cpp
uint64_t last_fire;    // derived bookkeeping — fine
uint64_t acc;          // sub-cycle fractional accumulator — fine
uint64_t my_own_now;   // NO — this is how skew bugs are born
```

If two components disagree about "now" by even one cycle, DMA finishes
early, PPU modes flip late, and the failure shows up three systems later in
a game that only works on real hardware. When you reach the NES/GBA/PS1
phases, the scheduler you build here becomes the backbone: DMA, timers,
APU channel frame counters, and GPU vblank are all just events on this
queue.

## Worked micro-example

Period-3 timer and period-5 UART share a master clock:

```text
queue after boot:  (3, timer#1) (5, uart#1)
dispatch (3,timer) -> log cyc=3 dev=timer; push (6,timer)
dispatch (5,uart)  -> log cyc=5 dev=uart;  push (10,uart)
dispatch (6,timer) -> log cyc=6 dev=timer; push (9,timer)
...
event order: t3 u5 t6 t9 t10 t12 ...  -- fully determined, forever
```

That log is your golden test data: identical input -> byte-identical log,
on every machine, forever.

## What you build here

- `01_event_queue` — the scheduler itself (ordering, ties, reentrancy,
  `run_until` boundary semantics).
- `02_toy_soc_events` — the fx8 toy CPU plus a timer and a UART converted
  into event-driven devices under one master clock.
- `90_debug` — a seeded tie-breaking defect to diagnose.
- `91_challenge` — convert the legacy ch13-style tick-driven down-counter
  to event scheduling, bit-exact against the original.
- `99_coding_test` — synchronize three fictional devices at different
  frequencies into an exact event-order log.
