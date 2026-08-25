# 03 — Boot runner

Headless integration exercise: wire core + devices + scheduler into the
curriculum §52 CLI and produce hashable output.

## CLI

```
ch49_03_boot_runner_runner --rom FILE --headless
    [--cycles N] [--frames N] [--trace FILE] [--hash-frame FILE]
    [--input-file SCRIPT]
```

- `--rom` — raw little-endian word image, loaded at RAM word 0; entry is
  word 0.
- `--cycles N` — dispatch budget on the master clock. `--frames N`
  contributes `N * 564480` cycles (60 Hz NTSC video frame); when both are
  given the larger budget wins. Default: 200000.
- `--hash-frame FILE` — receives the EVENT LOG (one line per boot /
  gpu / dma / cd / latch / milestone / halt event), hashed as FNV-1a 64.
- `--trace FILE` — per-instruction lines in canonical shape
  `pc=<hex8> op=<hex8> cyc=<n>`.
- `--input-file` — device script applied at cycle 0 after boot scheduling.

## Script grammar (input file)

```text
# comment to end of line
MASK <hex>     INTC mask write
DMA <words>    kick the DMA channel with <words> words (decimal)
GPUCMD <hex>   queue one GP0 command word (bits[19:0] = pixel count)
CDREAD         start one sector read (completes after 19968 cycles)
SPUON          enable the sample-period interrupt (IRQ9 every 768 cycles)
```

Script pokes land AFTER the boot events in insertion order, so at equal
timestamps the hardware-initialized chain wins the FIFO tie-break — the
same guarantee the real INTC gives its request lines.

## Exit status and stdout

`0` on success (halted or budget exhausted), `2` on usage/IO errors.
Stdout summary: `events=<n> cyc=<n> halted=<0|1> fnv64=%016llX`.
