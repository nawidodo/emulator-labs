# 91_challenge — Scanline Gradient Challenge

## Task

Wire the S33N bundle into the provided HDMA core and produce the exact
224-byte per-line effect buffer the reference solution emits. Two pieces
are stubbed in `91_challenge/challenge.hpp`:

1. `parse_channels(config, watch)` — turn the bundle's config text into
   up to 8 `ChannelSetup` values (see the grammar in `bundle.hpp`).
2. `build_effect_buffer(log, watch_reg, buf)` — fold the frame's ordered
   write log into `buf[n]` = value the watched register held after line n.

Everything else (bundle loader, HDMA core, runner CLI) is provided.

## Fixture

`fixtures/gradient.bin`: one direct channel writing $2100 every line,
224 one-byte entries, brightness ramping `min(15, n*16/224)`. Format
listing: `fixtures/gradient.format.txt`.

## Acceptance criteria

1. `ch33_91_challenge_tests` passes.
2. Running your built runner on the public fixture:

   ```bash
   ch33_91_challenge_runner \
       --rom templates/ch33_snes_dma_hdma_audio/91_challenge/fixtures/gradient.bin \
       --headless --frames 1 --hash-frame /tmp/grad.bin
   python3 tools/labs/hash_frame.py /tmp/grad.bin
   ```

   reproduces the reference FNV-1a-64 digest:

   ```
   FNV64 17F7C28EF777DE45
   ```

3. The same digest appears when the run is repeated (determinism).
4. `--frames 5` produces the identical hash to `--frames 1` for this
   table (the gradient is frame-invariant; a wrong `init()` placement or
   per-frame state leak breaks this).
5. `--trace /tmp/t.log` yields exactly 224 lines of
   `line=<n> chan=0 reg=2100 val=<hex>` in ascending line order.

## Hints

- The effect must be visible DURING each line; if your buffer looks like
  the golden shifted down by one, you reintroduced the 90_debug bug.
- `--hash-frame` bytes are RAW written values, not masked to INIDISP's
  4-bit brightness field.

The committed golden digest also lives in `fixtures/provenance.md` and in
tests/public/ch33_snes_dma_hdma_audio/.
