# ch40 — PS1 Interrupts and Timers

The PlayStation has no `HALT`. Games spend most of their time servicing
events that arrive on hardware schedules: the video signal ticks 60 times a
second (NTSC), three root counters free-run against system and video clocks,
DMA engines finish blocks, the CDROM delivers sectors. This chapter builds
the two devices that make all of that observable — the **interrupt
controller** and the **root counters** — plus the **deterministic event
scheduler** every later PS1 chapter hangs its timing on.

Primary reference: psx-spx "Interrupt Control" and "Timers" sections
(<https://problemkaputt.de/psx-spx.htm>). Everything in this chapter is
integer math: no wall clock, no threads, byte-identical replays.

---

## 1. The interrupt controller (`I_STAT` / `I_MASK`)

Two 16-bit registers in the I/O port region:

| Port        | Name     | Read            | Write                    |
|-------------|----------|-----------------|--------------------------|
| `1F801070h` | `I_STAT` | latched requests| **acknowledge**          |
| `1F801074h` | `I_MASK` | enable bits     | set enable bits directly |

psx-spx bit table (implemented verbatim by this chapter):

| Bit | Source | Notes |
|-----|--------|-------|
| 0   | VBLANK      | PAL 50Hz / NTSC 60Hz |
| 1   | GPU         | requested via GP0(1Fh), rarely used |
| 2   | CDROM       | also needs CD controller ack |
| 3   | DMA         | raised by DICR (ch43) |
| 4   | TMR0        | sysclk/dotclock root counter |
| 5   | TMR1        | sysclk/hblank root counter |
| 6   | TMR2        | sysclk/sysclk÷8 root counter |
| 7   | SIO0        | controller + memory card byte received |
| 8   | SPU         | audio events |
| 9   | PIO         | expansion port (lightpen reports) |
| 10  | SIO(2)      | second serial port |
| 11–15 | unused    | read as zero |

This chapter's fixtures drive the timer lines (bits 4–6) plus vblank;
the remaining sources arrive with their owning chapters but the controller
model already handles every one of them identically.
### Write-1-clears acknowledge

Writing `I_STAT` never sets bits:

```text
I_STAT = I_STAT & ~value      ; each 1 clears its bit, 0 bits untouched
```

This is the same idiom as the DMA interrupt register DICR you met in ch43.
A naive `status = 0` acknowledge compiles fine and destroys unrelated
pending sources — exactly the seeded bug of exercise `90_debug`.

### Level reassert

psx-spx describes the bits as edge-triggered latches: they are SET by a
false→true transition of the source line. That creates the famous ordering
rule for peripherals that must be acknowledged at their own port:

> First acknowledge `I_STAT`, then the device (e.g. `JOY_CTRL.bit4`).
> Doing it the other way round can lose an IRQ forever: the device re-arms
> before you clear the latch, so no new edge ever arrives.

Timers and vblank are *periodic* level-ish sources: if the raw line is
still asserted when software acknowledges, our model re-latches immediately
(`ack()` ORs still-held lines back). Acknowledging therefore only sticks
after the source itself is quiet — which is why real drivers service the
device first, or mask the line while clearing.

### The output side

COP0 sees a single wire:

```text
IRQ to CPU = (I_STAT & I_MASK) != 0
```

There is nothing else to it — COP0's CAUSE bit 10 is not even a latch; it
clears automatically as soon as `(I_STAT AND I_MASK)` becomes zero. All
policy lives in these two registers.

### Worked example

```text
raise(TIMER0)                 ; I_STAT = 0010h
raise(CDROM)                  ; I_STAT = 0014h  (device not serviced yet)
ack(TIMER0)                   ; CDROM line still held -> re-latches,
                              ; I_STAT stays 0014h: the ack did NOT stick
lower(CDROM); ack(0004h)      ; now it sticks: I_STAT = 0000h
```

Trace it yourself in `01_irq_controller` tests
(`irq_ack.still_asserted_line_relocks`).

---

## 2. Root counters 0–2

Three identical 16-bit up-counters at

```text
1F801100h + n*10h + 0   COUNTER   current value (R/W)
1F801100h + n*10h + 4   MODE      configuration + status flags (R/W)
1F801100h + n*10h + 8   TARGET    compare value (R/W)
```

Any write to MODE **forces COUNTER to 0** and restarts the internal
divider — this is how software starts/restarts a period.

### MODE register (psx-spx layout)

| Bits | Field | Meaning |
|------|-------|---------|
| 0    | SYNC enable | 0 = free run |
| 1–2  | SYNC mode   | per-timer meaning below |
| 3    | Reset after target | 0 = wrap at FFFFh, 1 = restart past TARGET |
| 4    | IRQ on target reach |
| 5    | IRQ on FFFFh wrap |
| 6    | IRQ repeat | 0 = one-shot until next MODE write |
| 7    | IRQ pulse/toggle | 1 = toggle line per event |
| 8–9  | Clock source | table below |
| 10   | IRQ request flag | reads 1 ("no request") after MODE write; flips per event in toggle mode |
| 11   | Reached target | sticky, cleared by reading MODE |
| 12   | Reached FFFF | sticky, cleared by reading MODE |

### Clock sources (MODE bits 8–9)

| Timer | 0        | 1           | 2         | 3         |
|-------|----------|-------------|-----------|-----------|
| 0     | Sysclk   | Dotclock    | Sysclk/8  | Hblank*   |
| 1     | Sysclk   | Hblank      | Sysclk/8  | Sysclk/8  |
| 2     | Sysclk   | Sysclk/8    | Sysclk/8  | Sysclk/8  |

\* the chapter adds hblank as Timer0 selector 3 so all three listed
"dotsources" are reachable; real silicon decodes 3 like 2. System clock ≈
33.87 MHz; dotclock ≈ 5.37 MHz (NTSC pixel rate); hblank fires once per
scanline.

### Target vs wrap

With bit 3 clear the counter counts through `TARGET` (firing bit 4's event
each pass) and keeps going until `FFFFh`, then wraps to 0 (bit 5's event).
With bit 3 set the period is exactly `TARGET+1` increments: the counter
reaches TARGET inclusive and restarts from 0 on the same tick.

### IRQ behavior

- Both conditions can be enabled; each fires independently.
- One-shot (bit 6 = 0): after the FIRST delivery, further events are
  suppressed until the next MODE write. The counter itself keeps running.
- Repeat (bit 6 = 1): every qualifying event is delivered.
- Pulse (bit 7 = 0): the request line pulses briefly per event — the
  latch in `I_STAT` holds until acknowledged.
- Toggle (bit 7 = 1): the request flag (readable as bit 10) flips on each
  event, and only the 1→0 transitions assert the line — so with repeat +
  toggle enabled, the interrupt controller sees every SECOND target/wrap
  event. Games use toggle mode to get a clean alternating state.

### Sync modes (MODE bit 0 = 1)

Driven by the video signal; deterministic because we feed exact levels:

| Mode | Counter 0 (Hblank)              | Counter 1 (Vblank)              | Counter 2        |
|------|----------------------------------|----------------------------------|------------------|
| 0    | pause during hblank             | pause during vblank             | stop forever     |
| 1    | reset at each hblank start      | reset at each vblank start      | free run         |
| 2    | reset at start + count only inside | same for vblank              | free run         |
| 3    | pause until first hblank, then free run | same for vblank          | stop forever     |

Timer 2 has no video sync: modes 0/3 halt it permanently (games use that
as a stopwatch gate), modes 1/2 behave like free run.

### Reading MODE

Bits 10–12 are live status, not storage. After a MODE write bit 10 reads
"no request" (1); in toggle mode it then alternates per event. Bits 11/12
latch "reached target"/"reached FFFF" and clear when you read MODE — the
classic polling idiom is:

```text
loop: lw   t0, T0_MODE
      bltz t0, seen       ; bit 11 set? (0x0800)
      nop
      j    loop
```

---

## 3. Deterministic scheduling

Root counters advance once per system cycle; instructions retire every ~2
cycles; the video signal changes state thousands of times per frame.
Coordinating that by hand invites off-by-one drift, so ch40 introduces the
event scheduler used by every later chapter:

```cpp
Scheduler sched;
sched.schedule(deadline_cycle, eventId, &callback, this);
...
sched.run_to(cycle);   // fires everything due, oldest first,
                       // same-cycle ties broken by insertion order
```

Properties that matter:

1. **No wall time.** Events carry absolute integer cycles. Two runs of the
   same program produce identical schedules — that is what makes golden
   traces possible at all.
2. **Periodic = self-rescheduling.** A callback that reschedules itself
   (`now()+1`) is a clock; `now()+200` an hblank generator.
3. **Ordering is total.** `(cycle, insertion sequence)` sorts every pair
   of events, including ones scheduled *while dispatching*.

The chapter machine wires it up like this:

```text
run_until(limit):
    while cycles + 2 <= limit:
        sched.run_to(cycles + 2)     # TICK at cycles+1 and cycles+2 fire:
                                     # sample video signals, tick counters,
                                     # raise I_STAT lines via edges
        cycles += 2
        execute one instruction      # LUI/ORI/ADDIU/LW/SW/BEQ/BNE subset
        emit trace line
```

Synthetic GPU timing (documented approximation, constant everywhere):

| Signal    | Period | Width | Notes |
|-----------|--------|-------|-------|
| hblank    | 200    | 40    | rising edge = hblank_pulse |
| vblank    | 5000 (=25×200) | 400 | level only |
| dotclock  | 6      | –     | one pulse every 6th sysclk cycle |

### Trace format

One line per retired instruction, canonical key=value shape consumed by
`tools/labs/compare_trace.py`:

```text
pc=80010020 op=8D6A0000 irq=0000 cyc=72
pc=80010020 op=8D6A0000 irq=0010 cyc=74
```

`irq=` is the raw latched `I_STAT` (not masked) at instruction retirement;
the poll loop above suddenly shows the moment Timer0's line appears.

### Worked end-to-end example (`03_scheduler`, sched0 fixture)

```text
cyc 12:  sw t1,4(t0)      ; MODE=0018h (reset@target|irq@target), target=60
cyc 14+: poll loop: lw/beq/nop, 6 cycles per iteration
cyc 72:  counter reaches 60 -> line rises -> I_STAT=0010h
cyc 74:  lw observes 0010h -> branch falls through
cyc 86:  sw 200h(zero) stores 0010h
cyc 88:  sw 0(t3) acknowledges; one-shot MODE means it stays clear
```

Golden traces live under `tests/public/ch40_ps1_interrupts_timers/traces/`
with generation commands in `provenance.md`.

---

## 4. Study checklist

- [ ] Recite the write-1-clears rule for `I_STAT` and why `status = 0` is wrong.
- [ ] Explain why the device-side handshake must come *after* the `I_STAT` acknowledge.
- [ ] Compute a period: MODE=0058h, TARGET=30 → how many sysclk cycles between IRQs?
- [ ] Predict what toggle mode does to your effective interrupt rate.
- [ ] Say which timers can count hblanks, which can count dots, and why Timer 2 cannot.
- [ ] Explain why a scheduler without total ordering cannot produce stable goldens.

(Answers are all in sections 1–3; the exercises grade you on them.)

## 5. Going further

- psx-spx Timers: <https://problemkaputt.de/psx-spx.htm#timers>
- psx-spx Interrupt Control: <https://problemkaputt.de/psx-spx.htm#interruptcontrol>
- External hardware tests: the community `ps1-tests` suite
  (<https://github.com/jonathanlinat/ps1-tests>) contains root-counter and
  interrupt cases that run on real hardware; our hidden manifest carries an
  optional `requires_rom` hook for them, but note the chapter machine
  executes a MIPS-I *subset* — full-suite coverage needs the ch38 CPU.
