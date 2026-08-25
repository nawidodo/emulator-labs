# Provenance — ch35 fixtures

- `programs/bouncer.bin`, hidden `programs/drift.bin` (+ `.asm.txt`
  listings): hand-assembled synthetic CHIP-8 programs, ch35 authoring
  (2026-08). No third-party or commercial content.
- Core semantics: templates/ch35_save_states_rewind_determinism/
  01_chip8_serialize/chip8.hpp (standard CHIP-8 subset, seeded xorshift).

## Golden generation

```bash
python3 tools/labs/generate.py --mode solution --force --targets ch35_save_states_rewind_determinism
cmake -S . -B build-solutions -DLABS_BUILD_SOLUTIONS=On && cmake --build build-solutions -j
R=build-solutions/solutions/ch35_save_states_rewind_determinism/01_chip8_serialize/ch35_01_chip8_runner
$R --rom tests/public/ch35_save_states_rewind_determinism/programs/bouncer.bin \
   --frames 30 --save-state /tmp/s.bin --frame-out /tmp/f.rgba
python3 tools/labs/hash_frame.py /tmp/s.bin --fnv-only   # state golden
python3 tools/labs/hash_frame.py /tmp/f.rgba --fnv-only  # frame golden
```

Run every command twice; outputs must be byte-identical before committing.

External suites: Timendus CHIP-8 test suite
(https://github.com/Timendus/chip8-test-suite) referenced by URL only;
gated via requires_rom + optional in manifests, never committed.
