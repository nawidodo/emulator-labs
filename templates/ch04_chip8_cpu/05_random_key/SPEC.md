# SPEC — 05_random_key details

## Deterministic RNG contract

`chip8::Chip8` owns a 64-bit LCG (Numerical Recipes constants):

```cpp
rng_state = rng_state * 6364136223846793005ULL + 1442695040888963407ULL;
return uint8_t(rng_state >> 56);   // top byte: best-mixed bits
```

- `reset()` restores `kRngSeed` (0x5EEDC0488).
- `seed_rng(uint64_t)` injects any fixed seed (tests use 1 and 0xFEED).
- Reference sequence for seed 1: 0x6C 0x82 0xA5 0x62 0xCB 0x80 ...

`CXNN` consumes exactly ONE `next_random()` per instruction and masks it
with NN. Golden files depend on this; do not change the constants without
regenerating everything (see tests/public/provenance.md).

## Key scripting (--input-file)

One line per executed instruction (not per frame - the CPU stage has no
frame loop). Each line lists pressed key nibbles, e.g. `14A`, or `.` for
none. Lines beyond the program length are ignored; running short means "no
keys". The committed fixture pairs `roms/random_key.bin` with
`input/random_key.input`.
