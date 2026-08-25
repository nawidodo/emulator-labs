# ch17 public fixture provenance

All fixtures are synthetic and generated deterministically by
`tools/make_programs.py` (this directory) — no RNG, no wall time, no
commercial ROM content:

    python3 tests/public/ch17_gameboy_audio_accuracy/tools/make_programs.py <repo-root>

## Files

| file | content |
|------|---------|
| `fixtures/wave_ram_pattern.bin` | 16-byte nibble ramp `0F 1E 2D ... F0` used by exercise 02/04 suites (duplicated byte-for-byte in the unit tests) |
| `fixtures/noise_lfsr_div0_s0_w15.txt` | exact polynomial table: first 64 raw LFSR output bits, divisor code 0 / s 0 / width 15 from 0x7FFF |
| `programs/square_melody.apuprog` (+ `.asm.txt`) | ch1: negative sweep pace1/slope2, 50% duty, decay env, four notes at t=0/140k/280k/420k |
| `programs/wave_arpeggio.apuprog` (+ `.asm.txt`) | ch3: RAM upload + NR32 100%, freq steps 100/200/400/800 with retriggers |
| `programs/noise_burst.apuprog` (+ `.asm.txt`) | ch4: three bursts covering width15 fast, width7 slow, high-s long |
| `programs/probe_debug.apuprog` (+ `.asm.txt`) | ch1: envelope decay + NEGATIVE-mode sweep probe (see 90_debug/DEBUGGING.md) |

The noise polynomial table is cross-checked bit-for-bit by the reference
solution suite (`ch17_03_noise_tests`) and was regenerated twice,
byte-identical. The wave pattern's FNV64 is C991CF3ABA9D09E5.

## Golden PCM hashes

Produced by the reference solution runner (run TWICE per program with
identical args; outputs compared byte-identical via `cmp`; FNV-1a-64
computed over the raw s16le stereo dump):

| program | command frames | FNV64 |
|---------|----------------|-------|
| square_melody | --frames 6 | 830366471C649495 |
| wave_arpeggio | --frames 4 | C6B3145D064517A5 |
| noise_burst   | --frames 3 | 604D079B620286FD |
| probe_debug   | --frames 5 | BD181515489B52AD |

Example generating command:

```bash
ch17_04_apu_runner \
  --rom tests/public/ch17_gameboy_audio_accuracy/programs/square_melody.apuprog \
  --frames 6 --headless --audio-out /tmp/square.pcm
python3 tools/labs/hash_frame.py /tmp/square.pcm --fnv-only
```
