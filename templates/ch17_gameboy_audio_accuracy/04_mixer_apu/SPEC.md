# 04 — Full APU: frame sequencer, mixing, downsampling, runner

Composes exercises 01-03 into a complete chip: register file FF10-FF3F,
frame sequencer, NR50/51/52 mixing and a deterministic 44100 Hz
downsampler feeding an s16le stereo ring buffer.

## Register map (writeReg)

| address | target |
|---------|--------|
| FF10-FF14 | ch1 NR10-NR14 |
| FF15 | ignored |
| FF16-FF19 | ch2 NR11-NR14 equivalents |
| FF1A | wave NR30 |
| FF1B | ignored (NR31 length code not modeled this chapter) |
| FF1C-FF1E | wave NR32/NR33/NR34 |
| FF20 | noise NR41 length code (bits 0-5) |
| FF21-FF23 | noise NR42/NR43/NR44 (FF23 bit 7 triggers) |
| FF24 | NR50 master volumes (L bits 0-2, R bits 4-6) |
| FF25 | NR51 routing (R ch1..4 bits 4-7, L bits 0-3) |
| FF26 | NR52 power; bit7 set = on, clear = powerOff() |
| FF30-FF3F | wave RAM (always writable) |

While powered down only FF26 accepts writes. Power-off clears every
register and channel state except wave RAM.

## Mixing

Active channels (enabled && DAC powered) map `analog=(out-7.5)/7.5`,
sum per side per NR51, scale by `(vol+1)/8`, clamp to [-1,1], multiply by
12000 -> int16. Frame layout: s16le interleaved L,R.

## Downsampling

Integer Bresenham, one accumulator advanced once per T-cycle:
`acc += 44100; while (acc >= 4194304) { acc -= 4194304; emit(mix()); }`
=> exactly 738 samples per frame, phase continuous across calls.

## Runner: ch17_04_apu_runner

Mandatory CLI shape (`--rom --headless --cycles N --frames N --trace FILE
--hash-frame FILE --input-file FILE`) plus chapter extension:

* `--audio-out FILE` — drains the mixed ring buffer to FILE as raw
  s16le stereo PCM (byte-identical content to what `--hash-frame` writes).

`--rom` loads a **.apuprog** program: little-endian records of
`u32 tcycleOffset, u16 regAddr, u8 value` (7 bytes), terminated by a
record with `regAddr == 0xFFFF`; addresses must fall in FF10-FF3F.
Simulation runs `total = max(--cycles, frames * 70224)` T-cycles; each
record is applied at its offset from cycle 0 (records past the end are
ignored). `--trace FILE` logs every applied record as
`t=<cycle> reg=<addr> val=<hex>` lines.

## Acceptance

`ch17_04_apu_tests` passes: power-off clearing (wave RAM preserved),
Bresenham sample counts (738/1476), silence when nothing active, NR51
routing + NR50 scaling arithmetic, three PCM-hash golden configs and the
coding-test setup echo test. `ch17_04_apu_runner --help` exits 0.
