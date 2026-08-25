# ch09_space_invaders_machine

The Space Invaders machine: the chapter 7–8 CPU composed with a real
board — memory map, port-mapped I/O, the TTL shift register, dual one-shot
vblank timers, column-major rotated VRAM and an RGBA8888 headless
renderer. Debugging exercise seeds a shift off-by-one and a VRAM
transposition; the coding test is board bring-up for the fictional
"Arcade-8080-B" from a spec.

## Layout

| Dir | Exercise | Artifact |
|---|---|---|
| `01_memory_map` | ROM/RAM/VRAM devices + range router, mirroring-absence rules | `ch09_01_memory_map_tests` |
| `02_shift_register` | exact 8-bit shifter, exhaustively tested | `ch09_02_shift_register_tests` |
| `03_io_ports` | input latches + scripted input protocol, sound event recorder, watchdog | `ch09_03_io_ports_tests` |
| `04_interrupts_frame` | vblank timers RST 08/10 alternation + cadence integration via trace | `ch09_04_interrupts_tests`, `ch09_04_cadence_runner` |
| `05_video_render` | VRAM→RGBA8888 renderer + full machine runner (`--frames`, `--hash-frame`) | `ch09_05_video_tests`, `ch09_05_si_runner` |
| `90_debug` | seeded bugs: shift amount off-by-one AND VRAM row/column transposition | `ch09_90_debug_tests` |
| `91_challenge` | course-original SI-compatible diagnostic: ports+interrupts+video in one boot | `ch09_91_challenge_tests`, `ch09_91_si_runner` |
| `99_coding_test` | Arcade-8080-B board bring-up from spec (two shifters, new map) | `ch09_99_si_b_tests`, `ch09_99_si_b_runner` |

## Gate checklist

- [ ] exercises: skel RED -> student GREEN (01–05)
- [ ] starter: chapter generates, builds, runs
- [ ] debug: fix both seeded bugs + produce bug-report.md
- [ ] challenge: diagnostic boots; final RAM state + frame hash match goldens
- [ ] coding_test: hidden manifest fully passing

## Runner CLI

```bash
ch09_05_si_runner --rom prog.bin [--cycles N | --frames N]
                  [--trace FILE] [--hash-frame FILE]   # raw RGBA8888 dump
                  [--dump-state FILE] [--input-file FILE] [--cpf N]
```

`--hash-frame` writes the final frame as raw RGBA8888 (224×256×4 bytes);
its FNV-64 digest is the golden frame hash. Input files carry one line per
frame: `P0 P1 P2` hex triplets. See SPEC.md for the documented hardware
model and port map.

## Verification

```bash
VERIFY_PREFIX=/tmp/labs-SI9 tools/labs/verify_chapter.sh ch09_space_invaders_machine
# [verify] verdict: skel_build=ok solutions=GREEN
python3 tools/labs/grade.py --repo . ch09_space_invaders_machine
```

Golden fixtures provenance: `tests/public/ch09_space_invaders_machine/provenance.md`.
