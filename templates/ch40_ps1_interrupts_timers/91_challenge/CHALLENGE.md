# CHALLENGE — ch40 / 91_challenge: two-period timer interrupt service

## Goal

Run a synthetic MIPS program on the chapter's deterministic machine. The
program configures root counter 0 in **repeat** mode, waits for TWO
interrupt periods, records each observed `I_STAT` sample plus a final
counter value into RAM, and spins forever.

## Acceptance criteria

1. `ch40_91_challenge_runner --rom tests/public/ch40_ps1_interrupts_timers/roms/challenge0.bin --cycles 600 --trace out.log --hash-frame out.txt`
   exits 0 and produces both files.
2. `out.log` matches the golden trace byte-for-byte:

   ```bash
   python3 tools/labs/compare_trace.py \
       tests/public/ch40_ps1_interrupts_timers/traces/ch40_91_challenge.log \
       out.log
   ```

3. The state digest hash equals the committed golden
   `fnv64=...` recorded in `tests/public/ch40_ps1_interrupts_timers/provenance.md`.
4. RAM after the run shows both period samples at `0x200`/`0x204`
   (`0x0010` — Timer0 line only) and a plausible post-ack counter sample at
   `0x208`.

## Program (challenge0.asm)

```text
target = 30, mode = reset@target | irq@target | repeat  (=0x58)
poll1: spin on I_STAT until nonzero, store to 200h, ack
poll2: same again, store to 204h, ack
final: read T0 COUNTER, store to 208h, spin
```

The second poll proves your controller re-arms: one-shot semantics would
hang here. It also proves the acknowledge path is clean — with the seeded
90_debug bug still present, the still-asserted line would re-latch and the
second period would be observed immediately instead of after a full
30-cycle period.

## Notes

- Entry point is `0x80010000`; the fixture loads at physical `0x10000`.
- Each instruction retires in exactly 2 system cycles; hblank every 200
  cycles; vblank every 5000.
