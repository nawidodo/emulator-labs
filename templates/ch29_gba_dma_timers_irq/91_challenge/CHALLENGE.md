# CHALLENGE — ch29: a deterministic DMA/timer race regression

## The problem

On real hardware, "the sound glitched for one frame, once" bugs are
notorious: timer overflows, HBlank DMAs and IRQ deliveries all race, and
host timing decides the winner. Emulators must NOT reproduce that
flakiness. Our answer: every event lives at an exact guest cycle, and
same-cycle ties resolve by scheduling (insertion) order — always.

## The fixture

`tests/public/ch29_gba_dma_timers_irq/fixtures/race_fixture.hws` arms a
period-5 timer (IRQ enabled) and issues register writes timed to collide
exactly with the timer overflow. The runner's event trace is the acceptance
artifact: byte-identical across runs and machines.

```bash
ch29_hw_runner --rom tests/public/ch29_gba_dma_timers_irq/fixtures/race_fixture.hws \
    --cycles 100 --trace /tmp/race.log --hash-frame /tmp/race.txt
python3 tools/labs/compare_trace.py \
    tests/public/ch29_gba_dma_timers_irq/traces/race_golden.log /tmp/race.log
```

The in-tree `main.cpp` replays the scenario programmatically twice and
asserts identical traces plus the committed state digest.

## Optional hardware gate

mGBA suite DMA/timer ROMs gate behind `requires_rom` + `optional`; drop
them under `roms/gba/mgba-suite/` to enable. Never commit ROMs here.
