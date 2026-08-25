# 01_psg_mix — SPEC

Implement legacy PSG primitives.

1. `psg_duty_high` — duty codes 0-3 -> 12.5/25/50/75% masks, MSB = phase 0.
2. `lfsr_step` — 15-bit LFSR, feedback bit0^bit1 into bit14, short-mode
   fold at bit 6; output 1 when resulting bit0 == 0.
3. `envelope_volume` — linear decay/increase per envelope clock, clamped,
   period 0 disables.
4. `psg_mix` — NR51 routing, NR50 side volumes (+1 scale), clamp ±511.
