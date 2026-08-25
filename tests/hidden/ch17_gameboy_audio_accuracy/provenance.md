# ch17 hidden fixture provenance

Synthetic .apuprog programs generated deterministically by
`tools/make_fixtures.py` (no RNG/time, no commercial ROM data):

    python3 tests/hidden/ch17_gameboy_audio_accuracy/tools/make_fixtures.py <repo-root>

| fixture | coverage |
|---------|----------|
| h1_square_sweep.apuprog | ch1 pace-2/slope-4 positive sweep, 75% duty, decay envelope; third note climbs until the SECOND sweep-update overflow silences it mid-note; asymmetric NR50=0x57/NR51=0x12 routing |
| h2_wave.apuprog | wave RAM checker upload, NR32 50% code, three frequency steps, right-side-only routing |
| h3_noise.apuprog | decaying burst, RISING-envelope burst (increase mode), frozen width-7 high-s burst |
| h4_coding_test.apuprog | the exact unseen configuration documented verbatim in templates/ch17_gameboy_audio_accuracy/99_coding_test/CODING_TEST.md |

Golden PCM hashes produced by the reference solution runner (run twice,
byte-identical), matching manifest.json (`--frames 4 --headless
--audio-out`, FNV-1a-64 over the s16le stereo dump):

| case | FNV64 |
|------|-------|
| h1_square_sweep | 72E43F50189780DD |
| h2_wave         | 0F7F90BFFDFD9929 |
| h3_noise        | 41CA2D76AD0DE6DD |
| coding_test_waveform_config | 316C3384F7782808 |

The optional mooneye `rapid_toggle.bin` case is an honest skip gated on a
student-supplied ROM; no such binary ships in this repository.
