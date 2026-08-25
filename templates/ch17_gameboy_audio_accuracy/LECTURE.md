# Chapter 17 — Game Boy APU I: channels, frame sequencer, mixing

The DMG audio processing unit mixes four synthesis channels into a stereo
output at a fixed digital sample rate. This chapter builds the whole APU
as a headless, cycle-deterministic C++ model — no host audio, no threads,
no floating-point phase. Determinism is everything: identical inputs must
produce byte-identical PCM so golden hashes are meaningful.

## The clock and the frame sequencer

Everything runs on the T-cycle clock, 4194304 Hz. One video frame is
70224 T-cycles (~59.7275 Hz).

The **frame sequencer (FS)** is the APU's heartbeat: one of its 8 steps
advances every 8192 T-cycles (512 Hz), so a full pass takes 16384 T-cycles
(256 Hz). Each step drives exactly one kind of decay event:

```text
step:   0    1    2    3    4    5    6    7
        |         |         |         |    |
length --+         |         |         |    |
sweep -------------+---------+---------+    |
envelope -----------------------------------+
```

* **Length** ticks on steps 0, 2, 4, 6 — 256 Hz.
* **Sweep** ticks on steps 2 and 6 — 128 Hz.
* **Envelope** ticks on step 7 only — 64 Hz.

Getting this cadence wrong is the classic "my emulator sounds too fast"
bug: an envelope that fires every fourth step instead of every eighth
audibly doubles decay speed even though every individual value looks sane.

## Channel conventions (committed for this chapter)

* All channels output the digital range **0..15**; a disabled channel
  outputs 0.
* Pulse duty rows are MSB-first bytes sampled with
  `sample = (form >> (7 - position)) & 1`, position 0..7:

| duty | row byte | pattern |
|------|----------|---------|
| 12.5% | `0x01` | 00000001 |
| 25%   | `0x03` | 00000011 |
| 50%   | `0x0F` | 00001111 |
| 75%   | `0x3F` | 00111111 |

  Note that under this formula a freshly triggered channel sits on a LOW
  step until the duty position walks into the high part of the row.
* Pulse frequency timer period = `(2048 - freq) * 4` T-cycles; wave timer
  period = `(2048 - freq) * 2`.
* Wave RAM is 16 bytes x two nibbles = 32 four-bit samples; even
  positions take the HIGH nibble. NR32 volume codes shift by {mute: 4,
  100%: 0, 50%: 1, 25%: 2}.
* The noise channel clocks a 15-bit LFSR at
  `divisor << s` T-cycles per step with divisor table
  `{8,16,32,48,64,80,96,112}`. The exact step is committed:

```cpp
int x = (lfsr ^ (lfsr >> 1)) & 1;
lfsr = (lfsr >> 1) | (x << 14);
if (width7) set bit 6 = x as well;
output = (~lfsr & 1) * volume;   // inverted bit0 AFTER the shift
```

  Exercise 03 commits an "exact polynomial table": the first 64 raw
  output bits for divisor code 0 / s=0 / width 15 from a fresh 0x7FFF
  register (`tests/public/ch17_gameboy_audio_accuracy/fixtures/
  noise_lfsr_div0_s0_w15.txt`). Any deviation from the formulation above
  diverges from that table within a handful of steps.
* Sweep arithmetic works on a shadow frequency. Candidates are computed
  one update ahead (at trigger and after every applied update) and applied
  on the following sweep tick; any candidate > 2047 disables the channel,
  negative candidates are discarded untouched. This reproduces the
  hardware's "overflow after the SECOND update disables" rule.

## DAC, mixing, NR50/NR51/NR52

Each active channel's output passes through a DAC centered halfway:

```text
analog = (out - 7.5) / 7.5          in [-1, 1]
```

A channel contributes only when it is enabled AND its DAC is powered
(NRx2 upper five bits nonzero for pulses/noise, NR30 bit 7 for wave);
dead channels leave their DAC centered, i.e. contribute nothing.

NR51 routes each channel into the left/right sums (bits 4-7 = ch1..ch4
right, bits 0-3 = left). NR50 holds per-side master volumes; side sum is
scaled by `(volume + 1) / 8`. The final float is clamped to [-1, 1] and
multiplied by 12000 (documented headroom factor below int16 clip) into
s16le interleaved L,R frames.

NR52 bit 7 is power. Powering off clears ALL registers (wave RAM is
preserved, as on hardware) and silences everything; while powered down
only NR52 itself accepts writes. Our chip starts powered DOWN — programs
must write FF26=0x80 before anything else.

## Downsampling: deterministic Bresenham

Real hardware emits samples continuously; we need a fixed 44100 Hz grid
without floating-point phase drift. The committed scheme is pure integer
Bresenham, one accumulator advanced once per T-cycle:

```cpp
acc += 44100;
while (acc >= 4194304) { acc -= 4194304; emit(mix()); }
```

Because the accumulator lives across calls, N frames always produce
exactly `floor(N * 70224 * 44100 / 4194304)` samples — 738 per frame,
1476 per two frames — on every platform, forever.

## Why hash PCM?

Audio bugs hide in plain hearing: an off-by-one envelope tick or a stale
sweep shadow can sound "almost right". Byte-exact FNV-1a hashes over the
mixed stream turn "sounds fine" into a pass/fail gate, which is what the
challenge, the debugging drill and the hidden coding test all use.

## References

- Pan Docs: "APU", "Frame sequencer", "Wave channel", "Noise channel"
  <https://gbdev.io/pandocs/Audio.html>
- Blargg's APU notes (dmg_apu.txt) for DAC/power semantics.
