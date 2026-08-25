# ch35_save_states_rewind_determinism

Versioned save states (machine state only), a GB-style multi-device state
registry, RLE-compressed rewind, and load-state determinism.

## Layout

| Dir | What |
|---|---|
| `01_chip8_serialize/` | Span-based writer/reader with schema-version byte over a full CHIP-8 core; headless runner with `--save-state/--load-state/--frame-out`. |
| `02_state_registry/` | Registry pattern: devices register named sections; aggregate blob with section table + version. |
| `03_rewind_ring/` | RLE compress/decompress + fixed-capacity ring of states + `step_back(n)`. |
| `90_debug/` | Seeded bug: serializer field-order drift — save/load silently corrupts timer fields. Write `bug-report.md`. |
| `91_challenge/` | save / load / rewind-10s on scripted input; rewind result must equal a from-scratch replay. |
| `99_coding_test/` | Load the same state 100 times → 100 identical framebuffer hashes. |

## Gate checklist

- [ ] exercises RED -> GREEN (`01`, `02`, `03`)
- [ ] debug: `bug-report.md`
- [ ] challenge: rewind-10s equals full replay
- [ ] coding_test: hidden manifest passes

## Fixture provenance

`tests/public/ch35_save_states_rewind_determinism/programs/` holds
hand-assembled synthetic CHIP-8 programs with `.asm.txt` listings; see
`provenance.md`. No commercial ROMs.

## Verification

```
VERIFY_PREFIX=/tmp/labs-ch35 tools/labs/verify_chapter.sh ch35_save_states_rewind_determinism
python3 tools/labs/generate.py --mode solution --force --targets ch35_save_states_rewind_determinism
cmake -S . -B build-solutions -DLABS_BUILD_SOLUTIONS=On && cmake --build build-solutions -j && \
  ctest --test-dir build-solutions --output-on-failure
python3 tools/labs/grade.py --repo . ch35_save_states_rewind_determinism
```
