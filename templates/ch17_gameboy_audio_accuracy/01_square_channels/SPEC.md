# 01 — Pulse channels (square)

Channels 1 and 2 share one design: a duty-waveform oscillator, a 64-step
length counter, a 64 Hz volume envelope and (channel 1 only) a frequency
sweep. Register set: NR10-FF14 for ch1, FF16-FF19 for ch2.

## Committed model

| seq | function | contract |
|-----|----------|----------|
| 1 | `trigger` | enable; reload length 0 -> 64; reload freq/envelope timers; sweep init + immediate first candidate (overflow disables) |
| 2 | `advance` | walk duty position every `(2048-freq)*4` T-cycles |
| 3 | `lengthTick` | 256 Hz; decrement when enabled; zero disables |
| 4 | `envelopeTick` | 64 Hz; period 0 freezes; step toward 15/0, clamp at bounds |
| 5 | `calcSweepCandidate` | `shadow +/- (shadow >> slope)` per negate bit |
| 6 | `sweepTick` | 128 Hz; apply pending candidate, compute the next one from fresh shadow; overflow (>2047) disables, negatives discarded |
| 7 | `sample` | disabled -> 0; else MSB-first duty bit times envelope volume |

Duty rows are `{0x01, 0x03, 0x0F, 0x3F}` sampled with
`(form >> (7 - position)) & 1`. Writing NR12 with bits 7-3 clear powers
the DAC off and disables the channel. The length counter is driven by
tests via trigger-reload semantics; full NR11-load plumbing arrives with
the CPU bus chapter.

## Acceptance

`ch17_01_square_tests` passes: duty walking, sample selection,
length/trigger interplay, envelope decay/rise/clamp sequences, sweep
overflow at trigger, second-update overflow disable, negative descent and
below-zero discard behavior.
