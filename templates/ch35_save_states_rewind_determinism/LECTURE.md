# Lecture 35 — Save States, Rewind and Determinism

A save state is a **complete, self-consistent snapshot of the emulated
machine** — everything that influences future behavior must survive a
save/load cycle, and nothing else may leak in.

## Machine state vs host state

```text
machine state                    host/frontend state
----------------------------     --------------------------------
CPU registers, RAM, VRAM         window handles, GL textures
timers, DMA channels             audio buffers, mixer queues
APU phase/accumulators           controller HAL state
cartridge RAM, RTC               filesystem paths, UI settings
RNG state (if the hardware
  has one — yours does!)
```

Only the left column goes into a state. Frontend resources are rebuilt
from it: on load you re-upload the framebuffer, re-seed the audio resampler
phase from APU state, reconfigure input mapping. States that embed window
handles or pointers are the classic beginner bug — they cannot be moved
between machines, processes, or even runs.

## Versioning

The first byte of every state you ship is a **schema version**. When you
add a field, bump the version and decide:

```text
v < current : reject cleanly ("state from older build") or migrate
v > current : reject — never guess at unknown layouts
```

Silent acceptance of a foreign layout is worse than rejection: the load
"succeeds" and the machine desyncs three minutes later. Padding bytes are
part of this story: a struct dumped with `memcpy` carries uninitialized
padding, so two identical logical states produce different bytes — and any
hash over the blob flips. Serialize fields explicitly, never whole structs.

## Span-based writers/readers

```cpp
class Writer {
    std::vector<uint8_t>& out;
    void u8(uint8_t v);  void u16(uint16_t v);  // little-endian
    void bytes(std::span<const uint8_t>);
};
class Reader {
    std::span<const uint8_t> in;
    bool u8(uint8_t& v); // false at end-of-buffer or bad version
};
```

Explicit field-by-field I/O costs nothing (states are tens of KB) and buys
portability forever.

## Rewind

Rewind is just save + load in a circle:

```text
every N frames: compress(state) -> ring[k]; k = (k+1) % capacity
step-back(n):   decompress(ring[(k-n) mod capacity]) -> restore
```

Ring capacity × capture interval = rewind horizon. 60 fps, snapshot every
10 frames, 60 slots ⇒ 10 seconds of rewind. Compression matters (a CHIP-8
state is ~6 KB; a GBA state ~280 KB) — here you implement RLE, which is
enough for tile-based graphics; real emulators use LZ4/zstd.

## Rollback (concepts)

Rollback netcode rewinds *on every received remote input*: restore state
S(t), apply the late input locally, fast-simulate t→now (~2–7 frames),
swap in the corrected frame. Requirements beyond rewind: deterministic
simulation (bit-exact given the same inputs) and cheap state copies. You
are building those exact primitives in this chapter.

## Deterministic replay

Same state + same input stream ⇒ bit-identical future. This fails if:

- anything reads the wall clock or an unseeded RNG,
- uninitialized memory participates in simulation,
- floating point varies across builds (integer guest clocks only — ch34).

Determinism is testable: **load the same state 100 times, run the same
input, all 100 framebuffer hashes must agree** — that is your coding test.

## Debugging non-determinism

Symptom: two runs diverge, or save→load changes behavior. Bisect by
hashing the serialized state every frame in both runs; the first hash
divergence localizes the dirty field. The seeded bug in `90_debug`
(uninitialized padding entering the blob) demonstrates the exact
methodology.
