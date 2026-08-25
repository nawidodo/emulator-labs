# provenance — ch31 golden trace (challenge)

- Artifact: `golden/challenge.trace` (mirrored at
  `tests/public/ch31_snes_65c816_cpu/traces/challenge.trace`)
- Program: `roms/challenge.bin`, hand-assembled two-bank listing in
  `roms/challenge.asm.txt` (bank 0 at file offset $00000, bank 1 at
  $10000, data marker word $BEEF at $12000).
- Generated with the reference solution executor:

```sh
runner --rom <exercise>/roms/challenge.bin --cycles 1000 --trace challenge.trace
```

- Executed TWICE, byte-identical (`cmp` clean) before committing.
- Pure function of the fixture and the documented reset state; no
  RNG, no wall clock.
