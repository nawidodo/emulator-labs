# Chapter 33 — SNES DMA, HDMA and Audio: Overview

Namespace for all code: `snesdma` (debug sub-exercise uses
`snesdma::debug`, coding test `snesdma::variant`). Chapter slug:
`ch33_snes_dma_hdma_audio`.

## What you build

A deterministic model of the SNES's data-motion machinery: the 8-channel
DMA engine with its canonical transfer-pattern table, the per-scanline
HDMA processor with correct at-line-start timing, a stub APU that consumes
uploads over comm ports `$2140-$2147` into a readable 64 KB APRAM, a
simplified DSP (per-voice volume + master volume, echo disabled), and an
integer three-domain scheduler under one 21.47727 MHz master counter.

## Exercise map

| Dir | Tests / runner | Topic |
|---|---|---|
| `01_dma` | `ch33_01_dma_tests` | DMA mode table, A-step, transfer sequences |
| `02_hdma` | `ch33_02_hdma_tests` | HDMA direct/indirect/repeat/terminate |
| `03_apu_sync` | `ch33_03_apu_sync_tests` | ports $2140-7, APRAM, DSP, clocks |
| `90_debug` | `ch33_90_debug_tests` | seeded bug: HDMA effect one line late |
| `91_challenge` | `ch33_91_challenge_runner` (+tests) | scanline gradient fixture |
| `99_coding_test` | `ch33_99_coding_tests` | unseen "transfer mode X" (SPEC.md) |

## Headless runner CLI

The challenge runner implements the mandatory chapter CLI. All flags are
ACCEPTED; no-op flags are documented in `--help`:

```text
--rom PATH          input bundle (S33N container; NOT a commercial ROM)
--headless          accepted, no-op
--cycles N          accepted, no-op (scanline-driven model)
--frames N          run N frames, artifacts reflect the final frame
--trace FILE        per-line effect log, lowercase key=value lines:
                    line=<n> chan=<c> reg=<hex> val=<hex>
                    (chapter deviation from pc=/op=/cyc= documented in
                    AUTHORING.md §Trace: HDMA logs have no PC)
--hash-frame FILE   raw 224-byte per-line effect buffer of the final frame;
                    graders hash these bytes with FNV-1a-64
--input-file FILE   accepted, no-op (no interactive input)
```

## S33N bundle format (`--rom` means input bundle)

```text
offset size field
0      4    magic "S33N"
4      1    version (=1)
5      3    reserved (=0)
8      4    config_len (LE32)
12     N    config text (see below)
12+N   4    blob_len (LE32)
16+N   M    blob: HDMA tables + indirect data, addressed by offset
```

Config text: one `key=value` per line; blank/`#` lines ignored.

```text
watch=RRRR         hex $21xx register tracked by --hash-frame
chN.enable=1       channel N present
chN.reg=RRRR       hex base B-bus register written each active line
chN.regs=K         consecutive registers per line (1..4)
chN.indirect=0|1   direct table vs indirect pointers
chN.bank=BB        hex bank byte for indirect addressing
chN.table=OOOO:LL  hex offset:length of the table inside the blob
```

## Gate checklist

- [ ] `01_dma`: all eight mode sequences exact; modes 6/7 force decrement.
- [ ] `02_hdma`: terminate / repeat / indirect / at-line-start tests green.
- [ ] `03_apu_sync`: upload round-trip, DSP math, domain tick counts.
- [ ] `90_debug`: bug found, fixed, `bug-report.md` written (bug / root
      cause / first divergence / fix / regression test).
- [ ] `91_challenge`: runner reproduces the golden gradient hash.
- [ ] `99_coding_test`: SPEC.md implemented; hidden grader filter passes.

## Verification

Recorded exactly as run on the authoring host (macOS arm64,
Apple clang C++20, `-Wall -Wextra -Wpedantic`):

```
VERIFY_PREFIX=/tmp/labs-SNES-ch33 tools/labs/verify_chapter.sh ch33_snes_dma_hdma_audio
[verify] prefix: /tmp/labs-SNES-ch33
[generate] skeleton 'ch33_snes_dma_hdma_audio' -> .../skel/tree/ch33_snes_dma_hdma_audio (34 files)
[verify] SKEL: build OK; ctest: 14% tests passed, 6 tests failed out of 7
         (red failures expected here)
[generate] solution 'ch33_snes_dma_hdma_audio' -> .../solution/tree/ch33_snes_dma_hdma_audio (34 files)
[verify] SOLUTIONS: GREEN — 100% tests passed out of 7
[verify] verdict: skel_build=ok solutions=GREEN
```

Skeleton tree builds and its tests run RED (stubs return wrong values);
solution tree builds clean and every test passes. Each hidden manifest
case was additionally executed by hand against the solution-built binaries
with the exact manifest arguments (see README section "Hidden-case
validation" below). Goldens in this chapter were produced by running the
reference solution twice and comparing bytes before committing.

### Hidden-case validation

Executed by hand from the repo root against the scratch solution binaries
(paths under /tmp/labs-SNES-ch33/solution/build/tree/ch33_snes_dma_hdma_audio/)
with the exact manifest arguments:

```text
case 1  dma_modes_suite
  <sol>/01_dma/ch33_01_dma_tests DmaModes
  -> exit 0, "8 tests, 0 failed assertions"          PASS

case 2  hdma_gradient_golden_hash
  <sol>/91_challenge/ch33_91_challenge_runner \
      --rom tests/hidden/ch33_snes_dma_hdma_audio/roms/hidden_ramp.bin \
      --headless --frames 1 --hash-frame {{tmp}}/frame.bin
  -> exit 0; fnv64(frame.bin) = EF3515111F192EED,
     matches manifest; two runs byte-identical       PASS

case 3  coding_test_variant_mode_x
  <sol>/99_coding_test/ch33_99_coding_tests VariantSpec
  -> exit 0, "5 tests, 0 failed assertions"          PASS
```

Skeleton-tree spot check: cases 1 and 3 exit non-zero on the stub build and
the case-2 runner emits a different digest — the hidden cases are RED for
skeletons exactly as intended. No `requires_rom` cases exist in this
chapter; every fixture is synthetic.
