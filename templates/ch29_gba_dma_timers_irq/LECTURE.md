# Chapter 29 — GBA DMA, Timers and Interrupt Controller

Primary reference: [GBATEK — DMA Transfer Channels](https://problemkaputt.de/gbatek.htm#dmachannels),
[Timers](https://problemkaputt.de/gbatek.htm#timers),
[Interrupts](https://problemkaputt.de/gbatek.htm#interrupts).
Optional test material: mGBA suite DMA/timer ROMs (student-supplied).

DMA, timers and the interrupt controller form the GBA's "nervous system".
They are also the source of the most emulator bugs that appear as
*rare, unreproducible glitches*, because their correctness depends on exact
event ordering. The cure is an event scheduler that works in guest cycles
and breaks ties deterministically.

## DMA channels

Four channels, each with SAD, DAD, count unit CNT and control word:

```text
bits  0-4   unused ( latch destinations)
bits  5-6   destination address control: inc, inc+reload, dec, fixed
bits  7-8   source address control:      inc,     -,      dec, fixed
bit  9      repeat (re-arm on trigger)
bit 10      transfer width: 0 = 16-bit, 1 = 32-bit
bit 11      game pak DRQ (prohibited, docs only)
bits 12-13  start timing: 0 immediate, 1 VBlank, 2 HBlank, 3 special
bit 14      interrupt on completion
bit 15      enable (auto-clears on completion unless repeat re-arms)
```

Channel capabilities differ: DMA0 may not target cartridge memory; DMA1/2
can feed the sound FIFOs (their special trigger); DMA3 can drive the EEPROM
(see ch30) and supports video capture (its special trigger). Word counts are
in units (words/halfwords), up to 0xFFFF, with the full 16 bits meaning
0x10000 on DMA3.

Priorities are fixed: DMA0 > DMA1 > DMA2 > DMA3. A running DMA cannot be
interrupted by a lower channel; requests landing in the same slice of time
are served lowest-number-first. Our simplified timing charges each transferred
unit a fixed cost (documented per exercise) and models "cycle stealing" as a
busy window during which other scheduled events are deferred — accurate
enough to reproduce ordering races, cheap enough to reason about.

Sound-FIFO DMA uses fixed source OR fixed destination with direction
control, transfers exactly four words per trigger and **reloads DAD to its
base value after every burst** — forgetting the reload is a classic bug that
corrupts audio after the first refill.

## Timers

Four 16-bit counters, each with:

```text
bits 0-1  prescaler: 1, 64, 256, 1024 cycles per tick
bit  2    cascade (increment on overflow of previous timer; ignores prescaler)
bit  6    interrupt on overflow
bit  7    enable
```

The counter counts *up*. On reaching 0x10000 it wraps to the reload value
(`TMxCNT_L`) and raises its flag. Period in ticks = `0x10000 - reload`.
Off-by-one errors here shift every sample a Direct Sound channel plays —
ch30 builds directly on this chapter.

## Interrupt controller

```text
IE   0x04000200  enable mask
IF   0x04000202  read = pending flags; write-1-to-clear acknowledges
IME  0x04000208  master enable
```

Hardware sets IF bits regardless of IE; the CPU sees an interrupt only when
`IME && (IE & IF) != 0`. Acknowledging is a write of ones clearing those
bits — the BIOS handler acknowledges before returning. HALT sleeps the CPU
until a pending enabled interrupt wakes it. Service priority is the lowest
set bit first.

## Event scheduling in guest cycles

Curriculum §52's headless requirement plus §56's stepping requirement meet
here: rather than ticking everything per-instruction, the reference design
keeps a priority queue of `(time_in_guest_cycles, callback)` entries:

```text
schedule(t, cb)          insert
advance(limit)           pop and run all events with time <= limit,
                         honoring bus-busy deferral
dma_burst(t, duration)   marks the bus busy; anything scheduled inside the
                         window fires when the burst ends (preemption)
```

Ties at identical timestamps resolve by insertion order — the property that
makes race regression fixtures (91_challenge) deterministic where real
hardware shows intermittent corruption.

## Debugging method

Race bugs hide. When a symptom appears "randomly", dump the event trace
(`--trace`) and find the FIRST divergence against a known-good run, exactly
like CPU tracing. The seeded defects in `90_debug` (HBlank DMA firing one
line late; timer period off by one tick) both produce clean, explainable
trace divergences.
