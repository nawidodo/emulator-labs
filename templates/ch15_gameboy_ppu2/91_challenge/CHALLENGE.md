# Challenge — three crafted scenes

Render all three committed v2 snapshots with the chapter runner and match
the golden frame hashes byte-for-byte:

```bash
./ch15_04_fullppu_runner \
    --rom ../../tests/public/ch15_gameboy_ppu2/snapshots/combo_scroll.ppu2 \
    --frames 3 --hash-frame /tmp/ch15_combo_scroll.rgba
python3 tools/labs/hash_frame.py /tmp/ch15_combo_scroll.rgba --fnv-only
# -> must equal the combo_scroll value in
#    tests/public/ch15_gameboy_ppu2/goldens/goldens.md

./ch15_04_fullppu_runner \
    --rom ../../tests/public/ch15_gameboy_ppu2/snapshots/combo_window.ppu2 \
    --frames 3 --window-off-lines 32:96 --hash-frame /tmp/ch15_toggle.rgba
python3 tools/labs/hash_frame.py /tmp/ch15_toggle.rgba --fnv-only
# -> must equal the combo_window[toggle 32:96] golden

./ch15_04_fullppu_runner \
    --rom ../../tests/public/ch15_gameboy_ppu2/snapshots/combo_signed.ppu2 \
    --frames 3 --hash-frame /tmp/ch15_combo_signed.rgba
python3 tools/labs/hash_frame.py /tmp/ch15_combo_signed.rgba --fnv-only
# -> must equal the combo_signed golden
```

## What each scene exercises

| snapshot | coverage |
|----------|----------|
| `combo_scroll.ppu2` | scrolled BG (SCX=103 SCY=57, wrap), window on $9C00 at WY=96, sprites over a checker BG incl. one priority-hidden sprite |
| `combo_window.ppu2` + `--window-off-lines 32:96` | the internal window-line counter: disabling lines 32..95 must SKIP 64 content rows — any LY-WY restart changes the hash |
| `combo_signed.ppu2` | $8800 signed tile addressing on both BG and window maps, mixed OBP0/OBP1 sprites |

`--frames 3` is deliberate: output must be deterministic across repeats.
A hash mismatch means one of your passes is subtly wrong — bisect by
rendering single scanlines in a scratch test until you find the first
diverging row, and compare against the mode trace (`--trace`) when timing
is suspect.
