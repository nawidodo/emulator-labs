# ch46_ps1_mdec — fixture provenance

Streams are synthetic, produced by the committed generator
`gen_stream.py` (deterministic; seed + macroblock count as arguments).
Each macroblock is six RLZ blocks (flat Q tables, scale 16); levels are
chosen exactly representable. No copyrighted data.

- `streams/pub.bin` — seed 1, 4 macroblocks.
- `streams/pub.rgba15` / `pub_hash.txt` — reference decode output and
  FNV-1a-64 hash (6D0D5B30487ECA5) from:

      build/skels/ch46_ps1_mdec/91_challenge/ch46_mdec_cli \
        --stream tests/public/ch46_ps1_mdec/streams/pub.bin \
        --out pub.rgba15 --hash-out pub_hash.txt

Executed twice on the reference tree; outputs byte-identical.
