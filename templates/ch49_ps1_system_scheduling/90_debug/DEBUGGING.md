# ch49 Debugging — one seeded defect

The bug lives in `90_debug/debug_scheduler.hpp` as an `@LABS` stub: the
STUB side carries the seeded BUG, the SOLUTION side is the corrected code.
Tests run RED with the seeded code and GREEN once fixed. Produce
`bug-report.md` containing:

```text
bug:
root cause:
first observable divergence:
fix:
regression test:   (the TEST name that would have caught it)
```

## Bug — SPU sample IRQ jumps the queue (`debug_scheduler.hpp`)

Symptom: in a boot where the CD read is kicked long before the colliding
SPU sample boundary, the event log at the shared deadline reads

```text
cyc=19968 evt=latch line=9 src=spu     <-- wrong order
cyc=19968 evt=cd_done lba=1
cyc=19968 evt=latch line=2 src=cd
```

instead of `cd_done`, `latch line=2 src=cd`, `latch line=9 src=spu`. Any
golden event-log hash from chapter fixtures fails; games polling IRQ flags
in a same-instant batch would observe interrupts in the wrong order.

Hint: both events carry the SAME timestamp. The comparator's first key is
therefore irrelevant for them — look at what the second key compares, and
at which of the two events was scheduled into the queue first (the CD
deadline at insertion cycle 0, or tick #26 scheduled at cycle 19200).
