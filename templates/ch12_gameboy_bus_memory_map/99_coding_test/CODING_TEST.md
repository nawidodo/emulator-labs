# Coding test — unseen simplified console "Tetra-8"

You have never seen this machine; the spec below is complete. Implement
it in `coding_map.hpp` (the three `TODO` bodies). The public tests in
`main.cpp` (`TEST(t8, ...)`) cover the spec examples; hidden grading
replays corner cases from the same document, so match it exactly.

## Methodology (how to attack an unseen map)

1. **Decode before you translate.** For every address, first decide
   WHICH region it hits using the table below; only then apply the
   region's offset math. Mixing the two steps is how alias bugs happen.
2. **Note the bus convention.** Tetra-8 idles LOW: dead slots read
   `$00` and short ROM images pad `$00`. Do not reuse the Game Boy or
   CourseBoy-II conventions.
3. **Test both directions.** Every alias must round-trip writes AND
   reads, at BOTH endpoints.

## Hardware spec

Two internal state items:
* `bank` — 2-bit ROM window select, resets to 1; writing 0 selects 1;
* power-on memory state is NUL-filled (`$00`).

### Map

| Range         | Rule |
|---------------|------|
| 0000-3FFF     | fixed ROM bank 0: `rom[addr]`; writes dropped |
| 4000-5FFF     | banked window: `rom[bank * $2000 + (addr - $4000)]`; past-image offsets read `$00` |
| 6000-9FFF     | closed: reads `$00`, writes dropped |
| A000-AFFF     | work RAM, 4 KiB, power-on `$00` |
| D000-D5FF     | shadow of work RAM: translation `addr - $3000` (D000->A000 ... D5FF->A5FF) |
| E000-E7FF     | scratchpad, 2 KiB |
| E800-EFFF     | mirror of the scratchpad, translation `addr - $0800` |
| FF00          | bank register: write normalizes (`val & 3`, 0 -> 1), read returns current bank |
| all else      | closed: reads `$00`, writes dropped |

### Spec examples

```text
R 0100        -> rom[$0100]
W FF00 03     ; bank := 3
R 4000        -> rom[$6000]
W FF00 07     ; masked to two bits: still 3
W FF00 04     ; 4&3 == 0 -> wraps to 1
R 4000        -> rom[$2000]
W A123 55
R D123 -> 55  ; shadow line
R D600 -> 00  ; beyond the shadow: closed slot
W E001 66 ; R E801 -> 66 ; scratch mirror
R BFFF -> 00  ; closed band reads NUL
```

## Acceptance

1. Visible suites green: bank normalization, windowed reads incl.
   NUL-shadow past a short image, RAM + shadow round-trips at both
   boundaries, scratch mirror, closed-slot sweep.
2. Hidden corner suites (`TEST(hidden, ...)`) pass against the same
   document.

## Hints

* The reset value of `bank` is 1 — the first windowed read happens
  before any register write in the hidden runs.
* Check your offset math against the IMAGE SIZE actually passed to the
  constructor, never against what a header claims.
