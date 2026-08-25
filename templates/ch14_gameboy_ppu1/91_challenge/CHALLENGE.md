# Challenge — complete background scenes

Render all three committed PPU snapshots and match the golden frame
hashes byte-for-byte using your exercise-05 runner:

```bash
./ch14_05_window_runner \
    --rom ../../tests/public/ch14_gameboy_ppu1/snapshots/plain_tiles.ppu \
    --frames 3 --hash-frame /tmp/plain.rgba
python3 tools/labs/hash_frame.py /tmp/plain.rgba --fnv-only
# -> must equal the value in
#    tests/public/ch14_gameboy_ppu1/goldens/plain_tiles.fnv

# repeat for scrolled_signed.ppu and window_scene.ppu
```

## What each scene exercises

| snapshot | coverage |
|----------|----------|
| `plain_tiles.ppu`   | identity BGP, unscrolled map, both $8000 tiles |
| `scrolled_signed.ppu` | SCX/SCY wrap past map edges, $8800 signed tile bytes, non-identity BGP |
| `window_scene.ppu`  | window on second map area, WX/WY gating, mid-map window rows |

`--frames 3` is deliberate: output must be deterministic across repeats.
A hash mismatch means one of your fetch paths is subtly wrong — bisect
by rendering single scanlines in a scratch test until you find the first
diverging row.
