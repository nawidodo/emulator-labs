# provenance — tests/public/ch40_ps1_interrupts_timers

All fixtures are synthetic programs assembled with a throwaway MIPS-I
assembler (lui/ori/addiu/lw/sw/beq/bne/nop subset, real branch delay
slots). No commercial ROM content is used anywhere in this chapter.

## roms/

| file | what it is |
|---|---|
| `sched0.bin` + `sched0.asm.txt` | 03_scheduler integration: configures Timer0 (target 60, reset@target, IRQ@target), polls `I_STAT`, stores the latched status at RAM `0x200`, acknowledges via write-1-clears, samples the counter to `0x204`, spins. Entry `0x80010000`, loaded at physical `0x10000`. |
| `challenge0.bin` + `challenge0.asm.txt` | 91_challenge: Timer0 repeat mode (target 30, mode `0x58`); records two interrupt periods at `0x200`/`0x204` and a post-ack counter sample at `0x208`. |
| `coding0.bin` + `coding0.asm.txt` | Config image for the coding test runner (`CTIM` format, see CODING_TEST.md): run 1500 cycles; t0 sysclk tgt=100, t1 hblank tgt=5, t2 sysclk/8 tgt=37; all reset@target + IRQ@target + repeat. A copy lives in `tests/hidden/ch40_ps1_interrupts_timers/roms/`. |

Assembling command (throwaway assembler, kept out of the repo):

```bash
python3 asm.py sched0.asm sched0.bin sched0.asm.txt
python3 asm.py challenge0.asm challenge0.bin challenge0.asm.txt
# coding0.bin is 12 hand-computed little-endian words, see coding0.asm.txt
```

## traces/

Generated with the ch40 reference solution tree
(`generate.py --mode solution`), run twice per artifact — byte-identical:

```bash
ch40_03_scheduler_runner --rom tests/public/ch40_ps1_interrupts_timers/roms/sched0.bin \
    --cycles 400 --headless --trace traces/ch40_03_sched0.log
#   -> fnv64(trace)      = 16337D4584345492

ch40_91_challenge_runner --rom tests/public/ch40_ps1_interrupts_timers/roms/challenge0.bin \
    --cycles 600 --headless --trace traces/ch40_91_challenge.log --hash-frame /tmp/h.txt
#   -> /tmp/h.txt contains fnv64=3C6CD50A7561AD22 (state digest)
#      fnv64(/tmp/h.txt) = 8565BA60D40858B7   <- hidden manifest value

ch40_99_coding_test_runner --rom tests/hidden/ch40_ps1_interrupts_timers/roms/coding0.bin \
    --headless --hash-frame /tmp/c.txt
#   -> /tmp/c.txt contains fnv64=17E98EA06C3D4FC2 (IRQ order log digest)
#      fnv64(/tmp/c.txt) = 8DE826F29F0574B5   <- hidden manifest value
```

Trace line shape: `pc=<8 hex> op=<8 hex> irq=<4 hex> cyc=<decimal>`
(one line per retired instruction, `irq` is raw latched I_STAT).

Hash algorithm: FNV-1a-64, offset basis `CBF29CE484222325`, prime
`100000001B3`, uppercase `%016X` — identical to `tools/labs/grade.py`.
