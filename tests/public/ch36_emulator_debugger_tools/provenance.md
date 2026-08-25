# Provenance — ch36 fixtures

- hidden `programs/glitch.bin`: hand-assembled synthetic fx8 program
  (ch36 authoring, 2026-08; no commercial content).
  Disassembly:
  ```
  00: 01 07   LDA #7
  02: 04 FE   ADD #$FE   ; a=5, c=1
  04: 03 20   STA $20
  06: 04 FF   ADD #$FF   ; a decrements each lap
  08: 03 20   STA $20
  0A: 07 04   JMP $04    ; deliberate infinite loop
  ```
- `sessions/demo.session`: scripted debugger command list for the ch36/91
  challenge transcript (commands only; the program is embedded in the
  challenge test source).

## Golden generation

```bash
python3 tools/labs/generate.py --mode solution --force --targets ch36_emulator_debugger_tools
cmake -S . -B build-solutions -DLABS_BUILD_SOLUTIONS=On && cmake --build build-solutions -j
D=build-solutions/solutions/ch36_emulator_debugger_tools/99_coding_test/ch36_99_diagnose
$D --rom tests/hidden/ch36_emulator_debugger_tools/programs/glitch.bin --result /tmp/dx.txt
python3 tools/labs/hash_frame.py /tmp/dx.txt --fnv-only   # -> 08C20787E51A8847
```

Run twice; byte-identical before committing (verified: DETERMINISTIC).
The diagnosis token is derived from live machine observations, so any
stepping/disasm/write-tracking defect changes the hash.
