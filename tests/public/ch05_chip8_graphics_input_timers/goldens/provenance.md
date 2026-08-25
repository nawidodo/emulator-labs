# Golden provenance — ch05_chip8_graphics_input_timers

All hashes below were produced by the REFERENCE SOLUTION (generate.py
--mode solution tree). Every generating command was run TWICE; both runs
produced byte-identical output before the digests were recorded.

Build once:

```bash
python3 tools/labs/generate.py --mode solution --force \
    --targets ch05_chip8_graphics_input_timers
cmake -S . -B build-solutions -DLABS_BUILD_SOLUTIONS=On
cmake --build build-solutions -j
RUNNER=build-solutions/solutions/ch05_chip8_graphics_input_timers/06_display_hash_runner/ch05_06_hash_runner
HASH="python3 tools/labs/hash_frame.py"
```

## Public goldens (this directory)

| File | Command | FNV64 |
|---|---|---|
| static_box_frames4.rgba | `$RUNNER --rom tests/public/ch05_chip8_graphics_input_timers/roms/static_box.bin --frames 4 --hash-frame /tmp/f.rgba` | `94C6312A5553EC25` |
| static_box_trace12.log | `$RUNNER --rom .../static_box.bin --cycles 12 --trace /tmp/t.log` | `934807974F7503EA` |
| moving_dot_frame_hashes24.txt | `$RUNNER --rom .../moving_dot.bin --frames 24 --frame-hashes /tmp/s.txt` | `67A5E5586552E83C` |
| keypad_probe_demo48.rgba | `$RUNNER --rom .../keypad_probe.bin --input-file roms/inputs_demo.txt --frames 48 --hash-frame /tmp/f.rgba` | `AC2B8FAB8A1C7005` |
| beep_counter_frames20.rgba | `$RUNNER --rom .../beep_counter.bin --frames 20 --hash-frame /tmp/f.rgba` | `80749653E42EEC3D` |
| beep_counter_beeps.txt | same command with `--beep-log /tmp/b.log`, then `$HASH /tmp/b.log` | `C6806F9B35441F9E` |

Fixture ROM provenance: every ROM in `tests/public|hidden/.../roms` and
`templates/ch05_.../91_challenge/fixtures` was hand-assembled for this
course (no commercial or third-party images); byte-level disassembly lives
next to each `.bin` as `.asm.txt`.

## Hidden manifest digests

Generated with the same runner from `tests/hidden/ch05_chip8_graphics_input_timers/`:

| Case | Command | FNV64 |
|---|---|---|
| dxyn_collision_frame_hidden | `$RUNNER --rom roms/overlap.bin --frames 3 --hash-frame f.rgba` | `405CE9998B29A7F5` |
| dxyn_clip_edge_hidden | `$RUNNER --rom roms/edge_clip.bin --frames 2 --hash-frame f.rgba` | `EB8156121A602505` |
| animated_frame_sequence_hidden | `$RUNNER --rom roms/orbit.bin --frames 24 --frame-hashes s.txt` | `FC833DCDB200FD7D` |
| keypad_scripted_feed_hidden | `$RUNNER --rom roms/keys.bin --input-file inputs/keys_feed.txt --frames 40 --hash-frame f.rgba` | `DE04A0D8FC8EDA31` |
| beep_log_hidden | `$RUNNER --rom roms/beeps.bin --frames 20 --beep-log b.log` | `C68DF79B354F9112` |

## Coding-test (99) digests

Built tool:
`build/skels/ch05_chip8_graphics_input_timers/99_coding_test/ch05_99_dxy0_tool`

| Case | Command | Expected |
|---|---|---|
| scroll_basic | `tool --pattern row_top --scroll 3 --hash-frame f.rgba` | stdout `vf=1`; FNV64 `B9D103FD6854A325` |
| scroll_zero_amount | `tool --pattern bottom_row --scroll 32 --hash-frame f.rgba` | stdout `vf=0`; FNV64 `0756ECD94C696C25` |
