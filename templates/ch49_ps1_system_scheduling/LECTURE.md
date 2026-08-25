# Chapter 49 Lecture — Complete PS1 System Scheduling

Everything you built in the PS1 phase — a core that executes, a GPU that
goes busy, DMA channels that drain, a CD drive with latency, an SPU
ticking at 44100 Hz — now runs on ONE master clock behind ONE event
scheduler. This chapter is the integration: no new subsystems, just the
discipline of making every device event-driven.

**A note on scope:** the full subsystems live in their own chapters and
cross-chapter imports are forbidden in this course's build layout, so we
define minimal *stand-in* devices inside this chapter — a tiny synthetic
RISC core, a GPU busy model, a DMA stall model, CD latency events, SPU
sample-period events, and an INTC latch. Each one reproduces exactly the
*timing behavior* that matters for scheduling; none pretends to be a full
implementation. The wiring patterns transfer 1:1 to the real devices.

## Study list

```text
master clock 33.8688 MHz
GPU dot clock 53.2224 MHz (= CPU x 11/7)
SPU 44100 Hz = 768 CPU cycles per sample
event queue / due-event dispatch
DMA stalls
GPU busy + STAT
CD latency -> IRQ2
SPU sample events -> IRQ9
INTC latch order
```

## The architecture

```text
CPU executes -> scheduler time advances -> due events dispatched ->
devices update    -> interrupt state changes    -> CPU continues
```

Concretely, nothing ever polls:

- The **CPU** is itself two events in flight: execute one instruction,
  schedule the next at `now + 4`. When DMA owns the bus it schedules the
  next instruction at the drain deadline instead — the stall becomes
  structure, not a spin loop.
- The **SPU** chain re-schedules itself from the PREVIOUS deadline
  (`deadline + 768`), never from "now". Anchoring on "now" stretches time
  whenever dispatch is delayed; anchoring on the deadline keeps sample
  boundaries exact forever.
- The **GPU** queues GP0 command words; a drain event retires them at the
  pixel ratio. Fractional ratios (11/7) ride in Bresenham integer
  accumulators — floating point never touches the timing path.
- The **CD** completes reads as pure latency events.
- Every state change lands in an append-only EVENT LOG; identical input
  yields a byte-identical log, which is what makes golden hashes possible.

## Clock ratios you must get exactly right

| Clock         | Value       | Relation to master            |
|---------------|-------------|-------------------------------|
| CPU (master)  | 33.8688 MHz | 1x                            |
| GPU dot clock | 53.2224 MHz | exactly 11/7 x CPU            |
| SPU sample    | 44100 Hz    | exactly 768 CPU cycles        |
| NTSC frame    | 60 Hz       | 564480 CPU cycles             |

Get one entry wrong and every golden hash in the chapter fails.

## DMA stalls

On real hardware a bursting channel owns the bus and the CPU freezes mid-
fetch. Emulators model this badly by "pausing" with counters. Here the
model is honest: while the drain event is pending, the next CPU event is
simply scheduled AT the drain deadline. Two events cannot overlap; the
stall duration is a property of the queue, not of anyone's bookkeeping.

## GPU busy state

GP0 writes do not execute synchronously — they enqueue. A drain event
executes queued commands over however many pixel-clock ticks they need,
STAT bit 28 reports busy so programs can poll, and an optional interrupt
fires when the queue goes idle. Games that write huge drawlists and then
wait on STAT exercise exactly this path.

## CD latency

A sector read is a deadline: kick at cycle T, complete at
T + kCdSectorCycles, raise IRQ2. Our stand-in uses 19968 cycles = exactly
26 SPU samples, chosen so CD completions CAN collide with sample
boundaries — which is precisely how we test interrupt ordering.

## Interrupt ordering guarantees

When multiple IRQs fire in the same dispatch batch, hardware latches its
request lines in a fixed sampling order. The scheduler reproduces this
with the FIFO tie-break: equal timestamps dispatch in insertion-sequence
order, always, regardless of heap internals. This single guarantee turns
"which interrupt fired first?" from a heisenbug into a pinned line in a
hashable log. The debugging exercise seeds exactly this defect; the coding
test replays scenarios that detect it.

## What you build here

- `01_scheduler_core` — schedule/cancel/dispatch-to-deadline with FIFO ties.
- `02_mini_devices` — stand-in SoC: core + GPU/DMA/CD/SPU/INTC, all
  event-driven, plus the mini assembler target your programs run on.
- `03_boot_runner` — headless runner: ROM boot to HALT, canonical traces,
  hashed event logs, device scripts.
- `90_debug` — diagnose the seeded wrong-order tie-break.
- `91_challenge` — boot a real assembled handshake program against the
  public golden event-log hash.
- `99_coding_test` — unseen programs/scripts, same hash contract.
