# Lecture — NES APU, DMA and Final Synchronization

An NES is five clocks in a trench coat. The CPU runs at master/12, the
PPU at master/4 (three dots per CPU cycle), the APU at master/24 for its
frame sequencer while its channel timers count CPU cycles. "Playing the
game right" means all of these advance in lockstep — this chapter builds
that lockstep and pins it with hashes.

## Pulse channels

Each pulse has a duty sequencer (4 waveforms x 8 steps), an 11-bit timer,
an envelope, a length counter, and a sweep unit.

- **Envelope**: a restart flag ("start flag") latched on any $4003-style
  write; on the quarter-frame clock it reloads the decay to 15 with the
  divider period taken from the volume field; otherwise the divider counts
  down the decay level toward 0, or wraps forever when the halt/loop bit
  is set. Constant-volume mode bypasses decay.
- **Length counter**: 32-entry load table (frames); decrements on
  half-frames unless halted; zero silences the channel.
- **Sweep, negate mode — EXACTLY**: target = period ± (period >> shift).
  The subtract direction uses a one's complement, so pulse 1 computes
  `period - delta` but pulse 2 computes `period - delta + 1`. This single
  bit of asymmetry is why identical sweep registers sound different on
  the two channels. Positive sweeps whose target exceeds $7FF are
  rejected (and mute the channel); periods below 8 mute unconditionally.

## Triangle

No volume — its "envelope" is the 32-step sequence 15..0 then 0..15. The
sequencer is gated by BOTH the length counter and the linear counter;
the linear counter reloads from its register on a reload flag and counts
down only when the control/halt flag is clear.

## Noise

A 15-bit LFSR, shifted on timer expiry with feedback = bit0 XOR bit6
(short mode) or bit0 XOR bit1 (normal), fed into bit 14. Periods come
from a 16-entry table. Output gates on shift-register bit 0.

## DMC basics

The output level is a 7-bit counter nudged by decoded delta bits (+2/-2).
Our course model keeps the level, rate timer, loop flag and a documented
STUB: when the sample buffer would need a memory fetch we expose
`needs_fetch()` and steal NO cycles. Level-trigger beyond that flag is
deliberately out of scope; everything hashed in this chapter behaves
identically with and without real fetches because the model says so.

## Frame counter

Course model, exact edges (CPU cycles): 4-step quarters at 7457, 14913,
22371, 29829; halves at 14913 and 29829; the IRQ asserts at 29829 unless
inhibited; wrap after 29830. 5-step mode adds an inert step, halves land
at 14913/37281, wrap after 37282, never an IRQ. $4017 writes reset the
sequencer immediately (documented simplification of the 3/4-cycle delay).

## OAM DMA: 513 or 514

A $4014 write stalls the CPU for one dummy cycle — two if the write
landed on an odd CPU cycle — then 256 read/write pairs: 513 cycles even,
514 odd. During ALL of them the PPU keeps catching up three dots per
cycle. That is how a sprite DMA nudges a raster split: the visual effect
is timing, not logic.

## The catch-up scheduler

One function owns time. Per CPU cycle it: begins/services OAM DMA,
advances the PPU exactly `kPpuDotsPerCpu` (3) dots stepping devices per
dot, ticks the APU once, dispatches quarter/half clocks on their fixed
edges, and lands one integer audio sample. Get the ratio wrong — say, 2
dots per cycle — and nothing crashes; instead every raster-sensitive hash
slowly drifts away from reality. That failure mode IS the coding test.

## Audio hashing

Mono s16le PCM, one sample per CPU cycle, mixed with pure integer math
(sum of channel outputs scaled by 512, saturating). Headless runs dump
raw PCM via `--audio-out`; goldens are FNV-1a 64 over the bytes.

## References

NESdev wiki: *APU Pulse*, *APU Triangle*, *APU Noise*, *APU DMC*,
*APU Frame Counter*, *OAM DMA*, *CPU/PPU frame timing*.
