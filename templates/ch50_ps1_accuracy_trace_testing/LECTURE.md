# Chapter 50 Lecture — PS1 Accuracy, Trace Testing and Compatibility

An emulator that boots a game is not the same thing as a correct emulator.
Commercial titles are tolerant: they retry DMA, poll timers instead of
trusting interrupts, and render garbage for exactly one frame before the
next one covers it. "It boots" tells you almost nothing about whether your
cycle counts, your GTE rounding, or your BCD arithmetic are right. Accuracy
testing is the discipline of replacing "it seems to run" with pinned,
compared, versioned evidence — and this chapter builds the tooling that
makes that evidence cheap enough to collect on every commit.

## Study list

```text
boots vs correct
golden trace testing
VRAM hash corpora
sample hash corpora
state pins
regression discipline
suite manifests
ps1-tests (external)
```

## The boots-vs-correct distinction

Three failure classes hide behind a booting game:

1. **Silent divergence** — the game works but your CPU trace differs from
   hardware in cycle counts. Nothing visible breaks until a different game
   times audio against your wrong clock.
2. **Latent breakage** — a fill that loses its last column or a blit that
   ignores stride looks fine in one title and shreds another. Only a
   corpus of pinned outputs catches it deterministically.
3. **Regression churn** — fixing bug A silently re-breaks bug B. Without
   per-bug test cases, accuracy work is whack-a-mole.

The cure for all three is the same: turn behaviour into bytes and compare
the bytes.

## Golden trace testing

A golden trace is a recording of architectural behaviour, executed by a
known-good implementation and committed next to the command that produced
it. Our CPU emits lines in the course-wide format:

```text
pc=00000008 op=02112000 cyc=3
pc=0000000C op=01220FFF cyc=4
```

`pc` is where the instruction was fetched; `cyc` counts guest cycles
including that instruction. Two properties make traces valuable:

* **Determinism** — running the fixture twice must produce byte-identical
  output. If it does not, you have hidden state, not an emulator.
* **First-divergence localization** — when comparing against a golden, the
  first differing line points at the exact instruction whose semantics,
  encoding decode, or cycle cost you got wrong.

Traces are hashed (FNV-1a 64) so a suite case can pin thousands of lines
with one constant — see `builtin.cpu_trace`, which runs `kCpuProgram`
twice, requires identity, and compares the digest.

## Hash corpora

Whole-framebuffer and whole-audio comparisons are the heavy artillery:

* **VRAM hash corpora** — draw a fixed scene, hash the entire VRAM surface.
  One wrong pixel changes the digest. This catches off-by-one fills,
  stride bugs, dither errors, format mistakes.
* **Sample-hash corpora** — render a fixed note, hash the PCM bytes. This
  catches envelope shape errors, interpolation seams, clamping bugs.

Hashes are not diagnostic (a mismatch tells you *that*, not *where*) — so
they pair with traces: hashes guard the whole surface, traces localize.

## Per-subsystem state pins

Some behaviour is too small for a frame and too stateful for a pure
function. For those we pin **end-of-operation register state**: after a DMA
block, madr/bcr/enable/irq must equal exact values; after N timer ticks,
cnt and the target-hit count are fixed numbers; after a CDROM sector walk,
LBA, count and data hash are pinned. These pins are cheap, precise, and
fail with names attached (`timer counter/target-hit state mismatch`).

## How public suites work

This chapter's public suites exercise only psx-mini. Real-hardware suites
exist and matter — the community-maintained
[ps1-tests](https://github.com/JaCzekanski/ps1-tests) collection ships
CPU (`cpu/*`, including coprocessor behaviour), GTE (`gte/test-all`,
`gte-fuzz`) and timing tests as PS-EXE binaries, and projects like Avocado
and DuckStation gate contributions on them. We reference them by URL only:
their binaries are downloaded separately and never enter this repo. The
hidden manifest carries an `optional` + `requires_rom` case showing the
gate pattern: when the student drops a real test binary under `roms/`,
grading picks it up; otherwise it skips gracefully without faking a pass.

## Regression discipline: every fix gets a pinned case

The rule that keeps emulators correct over years:

> Every bug fix lands together with a test case that fails on the old code
> and passes on the new code. No exceptions, including "obvious" fixes.

Chapter 50 makes you practice it ten times. `90_debug/regressions.hpp`
seeds ten historical-flavoured bugs — sign extension, trace pc ordering,
fill width, source stride, envelope quanta, DMA counts, chain enable
clearing, GTE shift placement, timer target compares, BCD decoding. Each
is verified by EXACTLY ONE named suite case (`seed01`..`seed10`), so a
diagnosed fix flips exactly one red case green. That mapping — bug ↔
case — is the unit of regression discipline; DEBUGGING.md documents every
seed's symptom.

## The `make accuracy` integration story

Every test this chapter registers carries the ctest label `accuracy`:

```cmake
set_tests_properties(... PROPERTIES LABELS accuracy)
```

which means the whole chapter's accuracy posture is one command from any
build directory:

```bash
ctest -L accuracy          # run only the accuracy-labelled tests
```

The repo-level `make accuracy` target is infrastructure-side sugar — it
simply forwards to `ctest -L accuracy` through the shared Makefile
(`CTEST_EXTRA += -L accuracy`). You do not need to touch shared files to
participate: adding the label to your tests IS the integration contract.
The aggregate runner ties it together end to end: it reads a suite
manifest, executes built-in checks and external binaries headlessly,
prints PASS/FAIL per line plus a summary, and exits 0 iff everything
passed — which is precisely what ctest, `make accuracy`, and the hidden
grader each consume.

## The reference architecture

```text
fixture program ──► MiniCpu ──► trace text ──► FNV-64 ──┐
draw calls      ──► MiniGpu ──► VRAM bytes ──► FNV-64 ──┤
render call     ──► MiniSpu ──► PCM bytes   ──► FNV-64 ──┼──► suite manifest
DMA/GTE/timer/CDROM ops ──► end-of-op state pins ──────┘        │
                                                            aggregate runner
                                                        exit 0 ⇔ all PASS
```

Everything processes in guest time, no host clocks, no RNG: the same
inputs must produce the same digests on every machine, forever.
