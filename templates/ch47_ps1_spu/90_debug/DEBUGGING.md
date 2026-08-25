# ch47 Debugging — two seeded defects

Both bugs live in `90_debug/*.hpp` as `@LABS` stubs. The tests run RED with
the seeded code and GREEN once you fix it. For each bug produce
`bug-report.md` containing:

```text
bug:
root cause:
first observable divergence:
fix:
regression test:   (the TEST name that would have caught it)
```

## Bug 1 — metallic ADPCM (`debug_adpcm.hpp`)

Symptom: streams using predictor filters 2–4 decode with a subtly wrong,
"metallic" timbre; filter 0 and 1 material sounds fine at block starts.
The first divergence appears exactly at the second block boundary.

Hint: the predictor is a 2-tap IIR. One of its taps must see history that
is one sample older than the other.

## Bug 2 — envelope off by one (`debug_adsr.hpp`)

Symptom: golden PCM dumps during decay are consistently one quantum lower
than the reference from the very first decay update. Attack and sustain
are unaffected.

Hint: compare your decay delta against `(level * step) >> 6` for a level
you know. What input does the buggy expression actually use?
