# provenance — ch31 golden trace (demo)

- Artifact: `golden/demo.trace` (mirrored at
  `tests/public/ch31_snes_65c816_cpu/traces/demo.trace`)
- Program: `roms/demo.bin`, hand-assembled listing in `roms/demo.asm.txt`.
- Generated with the reference solution executor:

```sh
python3 tools/labs/generate.py --mode solution --force \
  --targets ch31_snes_65c816_cpu --out /tmp/labs-SNES-sol31/tree
c++ -std=c++20 -I third_party/labstest -o runner \
  /tmp/labs-SNES-sol31/tree/ch31_snes_65c816_cpu/03_execute/runner_main.cpp
runner --rom <exercise>/roms/demo.bin --cycles 1000 --trace demo.trace
```

- The run was executed TWICE and both traces were byte-identical
  (`cmp` clean) before committing.
- No RNG, no wall-clock, no host paths influence the output: the trace
  is a pure function of roms/demo.bin and the documented reset state.
