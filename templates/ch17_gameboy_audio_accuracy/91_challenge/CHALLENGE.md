# Challenge — golden PCM programs

Mix all three committed .apuprog programs with your exercise-04 runner
and match the golden hashes byte-for-byte. Every hash was generated from
the reference solution twice (byte-identical) and is recorded in
`tests/public/ch17_gameboy_audio_accuracy/goldens/goldens.md`.

```bash
RUNNER=./ch17_04_apu_runner          # from build/skels or build dir
FIX=../../tests/public/ch17_gameboy_audio_accuracy/programs

$RUNNER --rom $FIX/square_melody.apuprog --frames 6 --headless \
        --audio-out /tmp/square.pcm
python3 tools/labs/hash_frame.py /tmp/square.pcm --fnv-only
# -> 830366471C649495

$RUNNER --rom $FIX/wave_arpeggio.apuprog --frames 4 --headless \
        --audio-out /tmp/wave.pcm
python3 tools/labs/hash_frame.py /tmp/wave.pcm --fnv-only
# -> C6B3145D064517A5

$RUNNER --rom $FIX/noise_burst.apuprog --frames 3 --headless \
        --audio-out /tmp/noise.pcm
python3 tools/labs/hash_frame.py /tmp/noise.pcm --fnv-only
# -> 604D079B620286FD
```

## What each program exercises

| program | coverage |
|---------|----------|
| `square_melody.apuprog` | ch1 duty + decay envelope + NEGATIVE-mode sweep, four notes, stereo routing FF25=0x11 |
| `wave_arpeggio.apuprog` | wave RAM nibble ramp upload, NR32 100% code, four frequency steps with retriggers |
| `noise_burst.apuprog` | three bursts: width15 fast clock, width7 slow burst, high-s long burst |

A mismatch means one of your sequencing layers diverges. Bisect by
dumping shorter runs (`--cycles N`) around the first event boundary where
the hash could change, and compare sample values by hand there.
`probe_debug.apuprog` (frames 5 -> BD181515489B52AD) additionally probes
the envelope/negative-sweep interaction from the debugging drill.
