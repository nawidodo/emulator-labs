# ch17_gameboy_audio_accuracy — Game Boy APU I: channels, frame sequencer, mixing

Builds the DMG audio processing unit as a headless, cycle-deterministic
model over the T-cycle clock (4194304 Hz), from individual channel
sequencers to a mixed, downsampled s16le stereo stream:

```text
01_square_channels  pulse duty/length/envelope/sweep sequencing
02_wave_channel     32-nibble wave RAM playback + NR32 volume codes
03_noise_lfsr       15/7-bit LFSR noise + exact polynomial table fixture
04_mixer_apu        frame sequencer, NR50/51/52 mixing, Bresenham
                    downsampling @44100 Hz, .apuprog runner
90_debug            two seeded defects: envelope off-by-one +
                    negative-mode sweep stale-shadow bug
91_challenge        three golden PCM programs
99_coding_test      unseen register configuration graded via PCM hash
```

## Gate checklist

- [ ] exercises: all suites green (`LABS=ch17_gameboy_audio_accuracy make skels && make test`)
- [ ] starter: `ch17_04_apu_runner --help` works; public programs render audio
- [ ] debug: 90_debug fixed AND `bug-report.md` written
- [ ] challenge: three golden PCM hashes match (see 91_challenge/CHALLENGE.md)
- [ ] coding_test: `make grade GRADE_TARGETS=ch17_gameboy_audio_accuracy` exits 0

## Committed conventions

Duty rows `{0x01,0x03,0x0F,0x3F}` sampled MSB-first with
`(form >> (7-p)) & 1`; frame sequencer steps every 8192 T-cycles with
length on 0/2/4/6, sweep on 2/6, envelope on 7; DAC analog =
`(out - 7.5) / 7.5`; mixing scale factor 12000 into s16le interleaved
L,R; integer Bresenham downsampling (`acc += 44100` per T-cycle against
4194304). The chip starts powered DOWN — programs must write FF26=0x80.

## Program format (.apuprog)

Little-endian records of `u32 tcycleOffset, u16 regAddr, u8 value`
(7 bytes each), terminated by a record with `regAddr == 0xFFFF`.
Generators and listings live in
`tests/public|hidden/ch17_gameboy_audio_accuracy/tools/`.

## Runner CLI

Mandatory shape (`--rom --headless --cycles --frames --trace --hash-frame
--input-file`). Chapter extension: `--audio-out FILE` drains the mixed
ring buffer as raw s16le stereo PCM. `--rom` loads a .apuprog program;
total simulation = `max(--cycles, frames * 70224)` T-cycles; `--trace`
logs applied records (`t=<cycle> reg=<addr> val=<hex>`).

Hash PCM dumps with `python3 tools/labs/hash_frame.py FILE --fnv-only`.

## Verification (recorded)

```text
VERIFY_PREFIX=/tmp/labs-Ch17 tools/labs/verify_chapter.sh ch17_gameboy_audio_accuracy
  SKEL:      build OK; ctest RED (5/6 failing as expected)
  SOLUTIONS: GREEN — 100% tests passed out of 6
grade.py scratch validation (solution binaries vs tests/hidden/
ch17_gameboy_audio_accuracy): 4/4 hidden cases PASS (+1 optional
requires_rom skip, mooneye absent).
Goldens generated twice from the reference solution runner,
byte-identical (cmp):
  square_melody f6 830366471C649495 / wave_arpeggio f4 C6B3145D064517A5 /
  noise_burst f3 604D079B620286FD / probe_debug f5 BD181515489B52AD /
  h1_square_sweep f4 72E43F50189780DD / h2_wave f4 0F7F90BFFDFD9929 /
  h3_noise f4 41CA2D76AD0DE6DD / h4_coding_test f4 316C3384F7782808
(see tests/public|hidden/ch17_gameboy_audio_accuracy/provenance.md)
