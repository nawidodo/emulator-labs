# 02_direct_sound — SPEC

Implement Direct Sound playback.

1. `SoundFifo::push`/`pop` — 32-byte ring; empty pops repeat the last byte.
2. `apply_dsound_volume` — codes 0/1/2 -> >>2/>>1/>>0, 3 mutes.
3. `bias_quantize` — clamp(mixed + 10-bit bias) to [0,1023], drop low
   resolution bits (SOUNDBIAS bits 14-15).
4. `render_pcm` — timer-clocked frames, B weighted x2, FNV-64 digest.

Acceptance: wrap-around, stale-sample hold, volume/bias math exact.
