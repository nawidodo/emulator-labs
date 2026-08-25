# SPEC — 01_irq_controller

Model the PS1 interrupt controller (`I_STAT` @ `1F801070h`,
`I_MASK` @ `1F801074h`, psx-spx "Interrupt Control") as a plain register
pair plus a CPU-visible IRQ output.

## Behavior under test

- `I_MASK` stores its written value directly; unmapped high bits are ignored.
- Peripheral lines latch into status via `raise()`; raising an already-set
  bit is harmless.
- Writing `I_STAT` acknowledges **write-1-clears**: each 1 bit clears its
  status bit, 0 bits change nothing.
- A source whose raw line is *still asserted* re-latches immediately after
  an acknowledge — acknowledging the latch does not service the peripheral
  behind it. Devices drop their line explicitly via `lower()` (JOY_CTRL.ACK,
  CDROM response read, ...).
- Periodic sources (vblank, timers) re-assert on every period by calling
  `raise()` again.
- The IRQ output to COP0 is `(I_STAT & I_MASK) != 0`.

## Source bit assignment (psx-spx)

| Line            | Bit | Mask  |
|-----------------|-----|-------|
| VBLANK          | 0   | 0x001 |
| GPU             | 1   | 0x002 |
| CDROM           | 2   | 0x004 |
| DMA             | 3   | 0x008 |
| Timer0          | 4   | 0x010 |
| Timer1          | 5   | 0x020 |
| Timer2          | 6   | 0x040 |
| Controller/SIO0 | 7   | 0x080 |
| SPU             | 8   | 0x100 |
| PIO/expansion   | 9   | 0x200 |
| SIO(2)          | 10  | 0x400 |

The chapter models all eleven sources; only the timer lines are driven by
this chapter's fixtures (GPU/CDROM/SPU/... arrive in later chapters).
