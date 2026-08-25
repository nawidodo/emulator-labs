# ch29_gba_dma_timers_irq — DMA, timers, interrupts, event scheduling

Build the GBA's timing subsystem the way emulators must: four DMA channels
with real trigger semantics, cascading timers with exact reload behavior,
the IE/IF/IME interrupt flow — all driven by a deterministic guest-cycle
event scheduler that models DMA bus preemption.

See `LECTURE.md`; GBATEK's DMA/timer/interrupt sections are normative.

## Exercises

| Dir | Topic |
|-----|-------|
| `01_dma_channels` | control decode, address stepping, immediate transfers |
| `02_dma_triggers` | HBlank/VBlank/FIFO triggers, repeat, arbitration |
| `03_timers` | prescalers, exact reload periods, cascade chains |
| `04_irq_controller` | IE/IF/IME raise-ack-wake-service flow |
| `05_scheduler` | guest-cycle event queue with DMA preemption + runner |
| `90_debug` | seeded race: HBlank DMA one line late + timer off-by-one |
| `91_challenge` | deterministic race regression fixture (same-cycle ordering) |
| `99_coding_test` | unseen spec: DMA3 video-capture special trigger |

## Fixtures

`.hws` hardware scripts (`tests/public/ch29_gba_dma_timers_irq/fixtures/`)
are sequences of `{u32 cycle, u32 addr, u16 value}` register writes applied
by the scheduler at exactly that guest cycle; terminator `cycle == 0xFFFFFFFF`.

## Gate checklist

```text
Exercises       all NN_* skel RED -> student GREEN
Starter         generate.py --targets ch29_gba_dma_timers_irq builds
Debug           90_debug bug-report.md + fixed race defects
Challenge       91_challenge same-cycle ordering trace matches golden
Code Test       hidden manifest cases pass on the solution tree
```

## Verification

```text
VERIFY_PREFIX=/tmp/labs-GBA2 tools/labs/verify_chapter.sh ch29_gba_dma_timers_irq
[verify] verdict: skel_build=ok solutions=GREEN
```

Golden traces generated twice by the solution-tree runner (identical);
see `tests/public/ch29_gba_dma_timers_irq/provenance.md`.
