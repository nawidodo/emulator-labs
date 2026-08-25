# ch05_chip8_graphics_input_timers

CHIP-8 graphics, input and timers: framebuffer model, DXYN XOR sprites with
collision and clip/wrap quirks, keypad state + scripted input feeds, 60 Hz
delay/sound timers with recorded beep hooks, deterministic execution drivers,
and the headless frame-hash runner used by every later chapter.

## Exercises

| Dir | Focus | Contract |
|---|---|---|
| `01_framebuffer` | Display struct | `clear/get/set` with bounds API |
| `02_dxyn_sprites` | DXYN | XOR draw, collision return, clip/wrap via `Chip8Quirks.wrapping` |
| `03_keypad` | key state | `Keypad`, `InputFeed` (line protocol), FX0A wait model |
| `04_timers` | 60 Hz timers | accumulator-driven `tick_cycles`, beep hook recorder |
| `05_run_for` | drivers | `run(n_cycles)` -> frames advanced; exact `run_for(cycles, timer_ticks)` |
| `06_display_hash_runner` | starter backend | full runner CLI incl. `--hash-frame`, `--frame-hashes`, `--beep-log` |
| `90_debug` | debugging | three seeded defects (collision flag, edge clipping, timer rate) + `DEBUGGING.md` |
| `91_challenge` | acceptance trio | display/keypad/beep test fixtures with golden hashes |
| `99_coding_test` | unseen spec | implement `DXY0` scroll-display pseudo-op per `CODING_TEST.md` |

Fixed course rates (documented in `machine.hpp` / `timers.hpp`): 600
instructions/second, 60 Hz timers, 60 fps -> exactly 10 instructions per
timer tick and per emulated frame. All golden hashes depend on these.

Frame convention for hashing: RGBA8888 expansion of the 64x32 framebuffer,
ON -> `FF FF FF FF`, OFF -> `00 00 00 00`; digest is FNV-1a 64 (same
algorithm as `tools/labs/hash_frame.py`).

Input-file protocol: one line per frame of held hex digits (`5`, `25F`),
`.` or blank = no keys; applying a frame replaces keypad state entirely.

## Gate checklist

- [x] exercises: skel RED -> solution GREEN (verify script)
- [x] starter: headless runner builds and runs in skeleton mode too
- [x] debug: `90_debug` seeded bugs + symptom docs; students write `bug-report.md`
- [x] challenge: `91_challenge` acceptance trio vs golden hashes
- [x] coding test: `tests/hidden/ch05_chip8_graphics_input_timers/manifest.json`
      (10 cases: 4 frame-hash, keypad feed, beep log, driver boundaries,
      3 DXY0 cases, 1 optional external-suite case gated by `requires_rom`)

No commercial ROMs anywhere; every fixture is a course-original program with
its `.asm.txt` disassembly beside it. Goldens were generated twice by the
reference solution (identical output); commands recorded in
`tests/public/ch05_chip8_graphics_input_timers/goldens/provenance.md`.

## Verification

Recorded results of the final verification run (see README tail after
verification completes):

```text
VERIFY_PREFIX=/tmp/labs-Ch05Chip8Gfx tools/labs/verify_chapter.sh ch05_chip8_graphics_input_timers
  [verify] SKEL: build OK; ctest: 20% tests passed, 8 tests failed out of 10
           (the two greens are the --help smoke tests)
  [verify] SOLUTIONS: GREEN - 100% tests passed out of 10
  [verify] verdict: skel_build=ok solutions=GREEN

Hidden manifest: 9/9 executable cases PASS against the reference solution
tree (10th case is requires_rom-gated Timendus, optional); all 9 fail
against unmodified skeletons as intended.
Grading commands recorded in tests/public/.../goldens/provenance.md.
