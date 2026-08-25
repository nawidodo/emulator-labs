# Provenance — tests/public/ch07_i8080_architecture

All fixtures in this directory are **course-original, hand-assembled** 8080
programs. No commercial or third-party ROM content is committed here.

## roms/ch07_diag.bin

- Author: chapter authoring (ch07_i8080_architecture), synthetic program
- Assembled by hand into the byte array embedded in
  `templates/ch07_i8080_architecture/91_challenge/main.cpp` (identical
  bytes); the `.asm.txt` listing next to this file is the source of record
- Purpose: straight-line arithmetic/logic diagnostic; result table at
  4000-4004, HLT after 156 T-states

## traces/ch07_diag.log

Generated with the reference solution runner:

```bash
python3 tools/labs/generate.py --mode solution --force \
    --targets ch07_i8080_architecture/03_cpu_core
cmake --build build-solutions -j --target i8080_runner   # solution tree
./build-solutions/ch07_i8080_architecture/03_cpu_core/i8080_runner \
    --rom tests/public/ch07_i8080_architecture/roms/ch07_diag.bin \
    --cycles 10000 \
    --trace tests/public/ch07_i8080_architecture/traces/ch07_diag.log
```

Run twice; both runs produced byte-identical output before commit.
