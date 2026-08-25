# Challenge — pass a real timer program

`timer_probe.bin` is assembled from `timer_probe.asm.txt` with the course
mini-assembler (see `tests/public/ch13_gameboy_timers_interrupts/fixtures/
provenance.md` for the exact command). It configures TMA/TAC through
direct stores, unmasks the timer line in IE, executes EI/HALT, and counts
interrupts in an HRAM cell ($FF80) from its ISR at $0050.

## Acceptance criteria

1. `ch13_91_timer_tests` is green: the probe program driven through the
   chapter machine counts exactly 6 interrupts over a 200000-T-cycle run,
   every overflow line pairs with an irq line, and the log/state formats
   are byte-exact.
2. `ch13_91_timer_runner --rom timer_probe.bin --headless --cycles 200000
   --trace irq.log --hash-frame state.txt` exits 0, prints a one-line
   summary, and produces the committed golden log under
   `tests/public/ch13_gameboy_timers_interrupts/goldens/`.
3. The hidden grader runs UNSEEN variants of the probe (different TMA/TAC/
   poll length) through your runner and hashes the interrupt log. Your
   model must be right, not tuned.

## Log format (exact)

One line per event plus one final state line; lowercase keys, uppercase
hex digits, whitespace-separated key=value tokens:

```
cyc=<n> tima_overflow
cyc=<n> irq vector=<hh> ime=<0|1>
state af=hhhh bc=hhhh de=hhhh hl=hhhh sp=hhhh pc=hhhh cyc=<n> \
halted=<0|1> trap=<0|1> if=hh ie=hh tima=hh tma=hh tac=hh
```

Events are emitted per instruction boundary at the boundary's END cycle
count; overflow-before-dispatch orders the two lines naturally.
