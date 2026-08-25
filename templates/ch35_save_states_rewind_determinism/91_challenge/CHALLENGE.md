# Challenge — save / load / rewind 10 seconds (ch35)

Build a `RewindSession` that plays the synthetic CHIP-8 "bouncer"
program with a scripted input stream and supports:

```text
save      snapshot the machine (schema-versioned blob)
load      restore it
rewind    step back exactly N seconds of emulated time
```

## Requirements

1. Capture a compressed state every 10 frames into a ring sized so the
   rewind horizon is EXACTLY 10 seconds: 60 fps ⇒ 600 frames of history
   ⇒ 60 slots. (`rewind.hpp` from exercise 03 is vendored here.)
2. After rewinding, resuming playback must produce **bit-identical**
   frames to a machine that never rewound but received the same inputs
   from that point on — rewind is invisible except for where you land.
3. Determinism check inside the acceptance test: replay-from-scratch and
   rewind+resume converge to the same state hash.

## Why this is the challenge

The three primitives interact: capture interval defines your time
quantum (you can only land on multiples of 10 frames), ring capacity
defines your horizon, and any non-determinism anywhere breaks
requirement 2. Real rewind implementations (RetroArch, bsnes) live or
die by exactly these parameters.

Implement the annotated stubs in `session.hpp`; the acceptance tests in
`main.cpp` pin everything else.
