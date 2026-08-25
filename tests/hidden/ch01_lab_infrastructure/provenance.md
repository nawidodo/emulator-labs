# provenance — ch01_lab_infrastructure golden data

All goldens in this chapter were produced by running the chapter's
reference solutions (the `--mode solution` trees) TWICE per variant and
asserting byte-identical output before recording any hash. No hash was
hand-invented.

## Fixtures

`fixtures/sample.bin` (also mirrored as `tests/public/.../sample.bin`,
36 bytes) is synthetic data assembled by this repository, not derived
from any commercial ROM:

```bash
python3 - <<'EOF'
data = (b'EMU-LABS/CH01\n'
        + bytes([0x00, 0x01, 0x7F, 0x80, 0xFF, 0xFE])
        + bytes(range(0x41, 0x51)))   # 'ABCDEFGHIJKLMNOP'
open('sample.bin', 'wb').write(data)  # 36 bytes
EOF
```

## Golden hashes embedded in template sources

- `templates/ch01_lab_infrastructure/03_generator_starter/test_generator.py`
  — `GOLDENS`: sha256 of every emitted file (manifest included) for
  `fixture_a/b/c` × all valid variants.
- `templates/ch01_lab_infrastructure/99_coding_test/goldens.hpp`
  — `ch01_goldens::kEntries`: FNV-1a-64 of every emitted file for the
  seven-level template (variants: none, todo 1..7, solution) and the
  three challenge sentinel fixtures (solution mode).

Generating command (reference generator run twice per variant):

```bash
python3 tools/labs/generate.py --mode solution --force \
    --targets ch01_lab_infrastructure/03_generator_starter --out /tmp/gold
# for each variant v in {none,1..N,sol}:
python3 /tmp/gold/ch01_lab_infrastructure/03_generator_starter/generate_skel.py \
    --template fixture_a [--todo v | --mode solution] --out /tmp/out1
python3 /tmp/gold/ch01_lab_infrastructure/03_generator_starter/generate_skel.py \
    ...same args... --out /tmp/out2
diff -r /tmp/out1 /tmp/out2   # must be empty; then record sha256/FNV-64
```

## Hidden-manifest expectations

`tests/hidden/ch01_lab_infrastructure/manifest.json` records the FNV-64
of the dumper's stdout-format dump over `fixtures/sample.bin`, produced by
running the reference dumper binary twice:

```bash
./build-solutions/solutions/ch01_lab_infrastructure/02_hex_dumper/ch01_02_hex_dumper \
    --file tests/hidden/ch01_lab_infrastructure/fixtures/sample.bin \
    --output /tmp/dump1.txt
# repeat to /tmp/dump2.txt, cmp equal, then record FNV-64 of the file
```
