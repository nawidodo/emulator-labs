# ch30_gba_audio_saves_accuracy — PSG, Direct Sound, saves, accuracy

Final GBA chapter: legacy PSG channels and Direct Sound FIFO playback with
deterministic audio hashing; SRAM/flash/EEPROM save media with state-exact
command machines; and the milestone headless deterministic suite runner.

See `LECTURE.md`; GBATEK's sound/cartridge sections are normative.

## Exercises

| Dir | Topic |
|-----|-------|
| `01_psg_mix` | duty, envelope, LFSR noise, NR50/NR51 mixing |
| `02_direct_sound` | FIFO A/B, timer-clock sampling, volume shift, SOUNDBIAS |
| `03_save_flash` | SRAM + flash command state machine (ID/erase/program/bank) |
| `04_eeprom_dmac` | EEPROM bit protocol over DMA3 halfword streams |
| `90_debug` | seeded bugs: flash program masking + bank addressing |
| `91_challenge` | deterministic audio hash through PSG+DirectSound mixer |
| `99_coding_test` | headless suite runner: isolate failing subsystem via filters |

## Gate checklist

```text
Exercises       all NN_* skel RED -> student GREEN
Starter         generate.py --targets ch30_gba_audio_saves_accuracy builds
Debug           90_debug bug-report.md + fixed save-media defects
Challenge       deterministic audio digest reproduced
Code Test       hidden manifest cases pass on the solution tree
```

## Verification

```text
VERIFY_PREFIX=/tmp/labs-GBA2 tools/labs/verify_chapter.sh ch30_gba_audio_saves_accuracy
[verify] verdict: skel_build=ok solutions=GREEN
```

Audio digests generated twice by solution binaries (identical); see
`tests/public/ch30_gba_audio_saves_accuracy/provenance.md`.
