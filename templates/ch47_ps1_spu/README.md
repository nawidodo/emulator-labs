# ch47 — PS1 SPU

Build the sound processing unit from the sample format up: ADPCM decode,
voice lifecycle, pitch, envelope, and the final mix with CD input and IRQ9.
Reverb is accepted-and-bypassed by design (see LECTURE.md).

## Layout

| Dir            | Exercise                                                        |
|----------------|-----------------------------------------------------------------|
| `01_adpcm`     | PSX ADPCM block decoder (exact published filter table)          |
| `02_voice`     | key on/off, block walking, loop/mute/end semantics              |
| `03_pitch`     | fractional pitch stepper + interpolation seam                   |
| `04_adsr`      | four-phase envelope over the published rate tables              |
| `05_mix`       | 24-voice mixer, register window, DMA, CD input, IRQ9 + runner   |
| `90_debug`     | two seeded bugs: predictor history order, envelope off-by-one   |
| `91_challenge` | render the bundled jingle to the public golden PCM hash         |
| `99_coding_test` | hidden ADPCM stream → PCM hash contract                       |

## Gate checklist

- [ ] exercises: all `ch47_*_tests` RED on skeleton, GREEN when solved
- [ ] starter: `LABS=ch47_ps1_spu make skels && make build && make test`
- [ ] debug: fix both seeded bugs, write `bug-report.md`
- [ ] challenge: `ch47_91_challenge_tests` GREEN
- [ ] coding_test: hidden manifest passes (`make grade GRADE_TARGETS=ch47_ps1_spu`)

## Verification

Verified with the isolated chapter harness:

```bash
VERIFY_PREFIX=/tmp/labs-ch47 tools/labs/verify_chapter.sh ch47_ps1_spu
# [verify] SKEL: build OK; red failures expected here
# [verify] SOLUTIONS: GREEN
# [verify] verdict: skel_build=ok solutions=GREEN
```

Hidden cases were additionally executed directly against scratch-built
solution binaries with the exact manifest arguments; see
`tests/hidden/ch47_ps1_spu/provenance.md`.
