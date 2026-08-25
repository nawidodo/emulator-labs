# Challenge — the acceptance trio

The chapter gate requires passing three classic CHIP-8 acceptance tests:
**display test**, **keypad test**, and **beep test**. Commercial test ROMs
are off-limits for this course, so all three programs below were assembled
for this course (byte-exact sources in `fixtures/*.asm.txt`). You pass by
reproducing the golden hashes with your own core.

## How to run

```bash
# display test: vertical stripes
build/skels/ch05_chip8_graphics_input_timers/06_display_hash_runner/ch05_06_hash_runner \
    --rom templates/ch05_chip8_graphics_input_timers/91_challenge/fixtures/display_test.bin \
    --frames 2 --hash-frame /tmp/dt.rgba
python3 tools/labs/hash_frame.py /tmp/dt.rgba
#   expected FNV64 2FFFC7BF7D608CB5

# keypad test: one DT-paced sample per key press; dot at (column, key*2)
ch05_06_hash_runner \
    --rom .../fixtures/keypad_test.bin \
    --input-file .../fixtures/keypad_input.txt \
    --frames 48 --hash-frame /tmp/kt.rgba
python3 tools/labs/hash_frame.py /tmp/kt.rgba
#   expected FNV64 092B3214C73A0A05

# beep test: ST=3 beep + delay-timer dot trail + end marker
ch05_06_hash_runner \
    --rom .../fixtures/beep_test.bin \
    --frames 16 --beep-log /tmp/bt.log --hash-frame /tmp/bt.rgba
python3 tools/labs/hash_frame.py /tmp/bt.rgba      # expected 18F166968F89B029
cat /tmp/bt.log                                    # beep_start then beep_end
```

(Replace `...` with the full fixture path; run from the repository root.)

## Acceptance criteria

| Test | Fixture | Passes when |
|---|---|---|
| Display | `display_test.bin` | final frame hash `2FFFC7BF7D608CB5` after 2 frames |
| Keypad | `keypad_test.bin` + `keypad_input.txt` | final frame hash `092B3214C73A0A05` after 48 frames; exactly 8 lit pixels |
| Beep | `beep_test.bin` | final frame hash `18F166968F89B029` after 16 frames; `--beep-log` shows exactly one `beep_start frame=0` and one later `beep_end` |

The same three checks run automatically as `ch05_91_challenge_tests`
(reference values in `main.cpp`).

## What each test exercises

- **Display** — DXYN XOR drawing in a loop, sprite data addressed via I,
  immediate skip-branches for loop control. A wrong collision flag or a
  clip/wrap mistake changes the hash.
- **Keypad** — FX0A wait-for-key driven by a scripted input feed, plus DT
  pacing so exactly one sample happens per press (a core that runs FX0A
  twice inside one held-key frame produces duplicate dots and fails).
- **Beep** — FX18 sets ST, the 60 Hz timer drains it (beep hooks fire at
  start/end), while the program polls DT to march a trail of dots. Timers
  running at any rate other than 60 Hz shift the trail and break the hash.

## Provenance

All fixtures are course-original; see `fixtures/*.asm.txt` for byte-level
disassembly. Goldens were generated twice by the reference solution runner
(identical output both times) — commands recorded in the chapter's
`tests/public/ch05_chip8_graphics_input_timers/goldens/provenance.md`.
