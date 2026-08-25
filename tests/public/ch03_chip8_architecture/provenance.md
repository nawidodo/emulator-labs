# Provenance — ch03_chip8_architecture goldens

All golden hashes and traces in this directory were produced by running the
chapter's REFERENCE SOLUTION (the `--mode solution` tree of
`templates/ch03_chip8_architecture`) **twice per artifact**; both runs were
byte-identical (`cmp` clean) before committing.

## Artifacts and generating commands

Run from the repo root with `$SOL` pointing at the built solution tree
(e.g. `build-solutions/solutions/ch03_chip8_architecture` after
`python3 tools/labs/generate.py --mode solution --force --targets
ch03_chip8_architecture && cmake -S . -B build-solutions
-DLABS_BUILD_SOLUTIONS=On && cmake --build build-solutions -j`):

### roms/

Course-original programs, hand-assembled for this course (annotated listings
in `roms/*.asm.txt`). No commercial or third-party ROM bytes.
`ibm_logo.ch8` is a course-original equivalent of the classic IBM-logo test:
it draws the word "CHIP" using only chapter-3 instructions plus the minimal
DXYN blit. Canonical copy lives in
`templates/ch03_chip8_architecture/05_ibm_logo/roms/`; the copy here is the
graded public fixture.

### traces/fetch_demo_trace.log

```bash
$SOL/02_fetch_decode/ch03_02_fetch_runner \
    --rom tests/public/ch03_chip8_architecture/roms/fetch_demo.ch8 \
    --headless --cycles 6 --trace traces/fetch_demo_trace.log
```

### traces/base_demo_trace.log

```bash
$SOL/90_debug/ch03_90_debug_runner \
    --rom tests/public/ch03_chip8_architecture/roms/base_demo.ch8 \
    --headless --cycles 12 --trace traces/base_demo_trace.log
```

### traces/debug_probe_golden.log

```bash
$SOL/90_debug/ch03_90_debug_runner \
    --rom templates/ch03_chip8_architecture/90_debug/roms/debug_probe.ch8 \
    --headless --cycles 6 --trace traces/debug_probe_golden.log
```

### traces/ibm_logo_trace.log and traces/ibm_logo_frame.rgba

```bash
$SOL/05_ibm_logo/ch03_05_logo_runner \
    --rom tests/public/ch03_chip8_architecture/roms/ibm_logo.ch8 \
    --headless --cycles 20 \
    --trace traces/ibm_logo_trace.log \
    --hash-frame traces/ibm_logo_frame.rgba
```

Golden framebuffer digest (raw RGBA8, `tools/labs/hash_frame.py`):

```text
FNV64  5CDB52515E40115D
SHA256 7dbe9b120a90b0668d48118eda55ceebdb89051de557e246f2195ceee74c5cd7
```

The in-tree unit test `challenge.ibm_style_logo_frame_is_stable` pins the
equivalent FNV-1a 64 over the raw monochrome buffer: `0x84AFB2AD021998CD`.

## Hidden-manifest hashes

Trace digests embedded in `tests/hidden/ch03_chip8_architecture/manifest.json`
were generated the same way from the solution tree:

| Case | Command summary | FNV64 |
|------|-----------------|-------|
| fetch_trace_golden | 02 runner, fetch_demo.ch8, `--cycles 6`, trace | `6B40B9915B9CA5DC` |
| disasm_listing_golden | 04 runner, base_demo.ch8, `--disasm` | `89F0CA597BD01B25` |
| debug_probe_trace_golden | 90 runner, debug_probe.ch8, `--cycles 6` | `B8C98451BB4AC9AD` |
| ct_call_ret_trace_golden | 99 runner, ct_call_ret.ch8, `--cycles 8` | `F7148A08D09DBA27` |
| ct_se_trace_golden | 99 runner, ct_se.ch8, `--cycles 6` | `9B9981E4A82D677F` |
| ct_sne_trace_golden | 99 runner, ct_sne.ch8, `--cycles 9` | `196FEF3121CE84F5` |
| ct_bnnn_trace_golden | 99 runner, ct_bnnn.ch8, `--cycles 8` | `7B9B555AD963C526` |
