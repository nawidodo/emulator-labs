# 02 — Wave channel

Channel 3 plays 32 four-bit samples from wave RAM at a programmable rate.
Registers: NR30 ($FF1A, DAC power bit 7), NR32 ($FF1C, volume code bits
5-6), NR33/NR34 ($FF1D/$FF1E, 11-bit frequency + trigger). Wave RAM lives
at $FF30-$FF3F and survives power cycling.

## Committed model

| seq | function | contract |
|-----|----------|---------|
| 1 | `trigger` | enable; position = 0; timer reloaded from `timerPeriod()` so position 0 plays exactly one full period |
| 2 | `advance` | while enabled, step position mod 32 every `(2048-freq)*2` T-cycles |
| 3 | `sampleNibble` | even position: high nibble; odd: low nibble |
| 4 | `sample` | gated by enabled && DAC; volume-code shift table {mute:4, 100%:0, 50%:1, 25%:2} |

The committed wave RAM fixture is the distinctive nibble ramp
`0F 1E 2D 3C 4B 5A 69 78 87 96 A5 B4 C3 D2 E1 F0`
(`tests/public/ch17_gameboy_audio_accuracy/fixtures/wave_ram_pattern.bin`,
duplicated byte-for-byte in the unit suite).

Note the deliberate deviation-from-brief detail: the brief's "timer to 0"
is realized as loading the full period, so the first sample is audible
for a complete period instead of one T-cycle.

## Acceptance

`ch17_02_wave_tests` passes: nibble decode, trigger rewind, programmed
step rate with wraparound, all four volume codes, DAC-off gating.
