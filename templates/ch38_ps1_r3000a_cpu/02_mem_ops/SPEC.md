# SPEC — ch38 exercise 02: loads, stores, unaligned pair

Five blocks in `memops.hpp`:

1. `do_lw` / `do_sw` — aligned word access.
2. `do_load_byte` / `do_sb` — sign-extension decision.
3. `do_load_half` / `do_sh` — same at 16 bits.
4. `do_lwr` / `do_lwl` — the unaligned load pair (little-endian rules).
5. `do_swr` / `do_swl` — the unaligned store pair.

## Hardware context

- The R3000A is little-endian; KUSEG (0x00000000), KSEG0 (0x80000000) and
  KSEG1 (0xA0000000) views of RAM all resolve to the same bytes — the tests
  exercise the mirrors through one `Bus`.
- Misaligned `lw/lh` would trap with AdEL on hardware; the lwl/lwr pair is
  the sanctioned way to touch unaligned data without ever issuing a misaligned
  cycle. Each half-instruction does only aligned word accesses plus masking.
- Canonical sequences: `lwr rt,0(x); lwl rt,3(x)` and
  `swr rt,0(x); swl rt,3(x)`.

## Done when

`ch38_02_mem_ops_tests` passes; the round-trip test stores/loads unaligned
words at every byte offset correctly.
