# LECTURE — Whole-Machine Integration

Subsystems passing isolated tests is necessary but not sufficient. The
playable gate exercises the three integrations where real machines break:

1. **Clock domain joining.** The CPU consumes cycles; PPU dots advance at
   3x; APU samples land on their own period; OAM DMA steals CPU cycles.
   One master timeline (ch24 scheduler pattern) must dispatch all of them
   in order.

2. **Signal chain completion.** Controller strobe -> $4016 reads -> game
   logic -> PPU register writes -> pixels; and APU register writes ->
   timers/envelopes -> mixer -> PCM. Both chains cross at least four of
   your subsystems; a single wrong latch order breaks gameplay while every
   unit test stays green.

3. **Determinism under interaction.** Same ROM + same input stream must
   produce identical frames, audio, and final state — every run. This is
   the property that makes golden regression, replay, and (later) rollback
   possible at all.

Passing this gate means you can explain the full path:
input -> CPU -> device state -> emulated time -> observable output.
