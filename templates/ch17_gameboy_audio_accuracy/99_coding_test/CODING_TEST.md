# ch17 Coding Test — unseen APU configuration

The hidden suite runs YOUR `ch17_04_apu_runner` on an **unseen**
register-write program and FNV-1a-64 hashes the resulting PCM. The full
configuration is documented below — no pattern matching will save you;
only a correct frame sequencer, channel set and mixer reproduce the
hash. The same event list is encoded in
`tests/hidden/ch17_gameboy_audio_accuracy/fixtures/h4_coding_test.apuprog`.

## The configuration (register write table)

| tcycle | reg | value | meaning |
|--------|-----|-------|---------|
| 0 | FF26 | 0x80 | power on |
| 0 | FF24 | 0x77 | master L/R volume max |
| 0 | FF25 | 0xBF | routing: R ch1,ch2,ch4 / L ch1,ch2,ch3,ch4 |
| 8 | FF11 | 0x40 | ch1 duty 25% |
| 8 | FF12 | 0x74 | ch1 volume 7, decay, period 4 |
| 8 | FF13 | 0xEE | ch1 freq lo (with FF14 hi -> freq = 0x6EE) |
| 8 | FF14 | 0x86 | ch1 trigger |
| 16 | FF16 | 0x80 | ch2 duty 50% |
| 16 | FF17 | 0x83 | ch2 volume 8, increase, period 3 |
| 16 | FF18 | 0x22 | ch2 freq lo |
| 16 | FF19 | 0x85 | ch2 freq hi 2 + trigger (freq = 0x122) |
| 24 | FF30..FF3F | see below | wave RAM upload |
| 24 | FF1A | 0x80 | wave DAC on |
| 24 | FF1C | 0x40 | wave volume code 50% |
| 24 | FF1D | 0x00 | wave freq lo |
| 24 | FF1E | 0x84 | wave trigger (hi bits carry 100b -> freq = 0x400) |
| 32 | FF21 | 0xF2 | noise volume 15, decay, period 2 |
| 32 | FF22 | 0x27 | noise s=2, divisor code 7, width 15 |
| 32 | FF23 | 0x80 | noise trigger |

Wave RAM bytes (FF30..FF3F in order):
`01 23 45 67 89 AB CD EF FE DC BA 98 76 54 32 10`

The graded command is:

```bash
ch17_04_apu_runner \
    --rom tests/hidden/ch17_gameboy_audio_accuracy/fixtures/h4_coding_test.apuprog \
    --frames 4 --headless --audio-out {{tmp}}/coding.pcm
```

and the manifest checks FNV-1a-64 over that file.

## Preparing

1. Implement every `TODO(n)` in exercises 01-04.
2. Rehearse the setup with the public echo test:
   `LABS=ch17_gameboy_audio_accuracy make skels && make build`, then run
   `ch17_04_apu_tests coding_test` — it applies this exact sequence via
   `writeReg` and verifies the register echoes (`ch1.freq == 0x6EE`,
   `wave.waveRam[5] == 0xAB`, `noise.nr43 == 0x27`, ...).
3. Verify your runner end-to-end against any public program from
   `91_challenge`; if those hashes match, the hidden configuration only
   exercises combinations you have already proven.
