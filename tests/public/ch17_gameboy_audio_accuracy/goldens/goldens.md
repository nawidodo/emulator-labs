# ch17 golden hashes (reference solution)

All values are FNV-1a-64 over the raw s16le interleaved PCM dump written
by `--audio-out`. Generated from the reference solution twice per entry;
outputs were byte-identical both times.

## Public programs (91_challenge)

| program | frames | FNV64 |
|---------|--------|-------|
| square_melody.apuprog | 6 | 830366471C649495 |
| wave_arpeggio.apuprog | 4 | C6B3145D064517A5 |
| noise_burst.apuprog   | 3 | 604D079B620286FD |

Debugging-drill probe (90_debug/DEBUGGING.md):

| stream | frames | FNV64 |
|--------|--------|-------|
| probe_debug.apuprog | 5 | BD181515489B52AD |

## Unit-suite PCM configs (embedded in ch17_04_apu_tests)

| config | cycles | FNV64 |
|--------|--------|-------|
| pulse_envelope_config_hash | 2 frames | 84BA0E7DBE15C45D |
| wave_pattern_config_hash   | 1 frame  | 169A8728693FBD9D |
| noise_burst_config_hash    | 1 frame  | 941B79ED77D32665 |

## Debugging drill probe stream (ch17_90_debug_tests)

| variant | FNV64 |
|---------|-------|
| reference (solution) | EF2E9FBF072A032B |
| bugged (skeleton)    | 43D2649EDFE60D40 |
