# Provenance — ch34 fixtures

- `programs/hello_loop.bin` (+ `.asm.txt`): hand-assembled synthetic fx8
  program, written by the ch34 author (2026-08). No third-party or
  commercial content.
- fx8 ISA reference: templates/ch34_scheduler_architecture/02_toy_soc_events/fx8.hpp.

## Golden generation commands

Goldens are produced by the reference solution runner, run twice to confirm
byte-identical output:

```bash
python3 tools/labs/generate.py --mode solution --force --targets ch34_scheduler_architecture
cmake -S . -B build-solutions -DLABS_BUILD_SOLUTIONS=On && cmake --build build-solutions -j
./build-solutions/solutions/ch34_scheduler_architecture/02_toy_soc_events/ch34_02_toy_soc_runner \
    --program tests/public/ch34_scheduler_architecture/programs/hello_loop.bin \
    --cycles 400 --events /tmp/ev.log --trace /tmp/tr.log
python3 tools/labs/hash_frame.py /tmp/ev.log --fnv-only   # event-log golden
```

External suites: none required. Mooneye GB timer tests
(https://github.com/Gekkio/mooneye-test-suite) are referenced by the hidden
manifest as a `requires_rom`, `optional` case only.
