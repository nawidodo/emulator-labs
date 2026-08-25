# Challenge — CPU-level acceptance run

You have a full (subset) 65C816 now. The challenge program
`roms/challenge.bin` (listing: `roms/challenge.asm.txt`) is a two-bank
image that exercises the pieces this chapter built:

1. XCE into native mode, REP/SEP width switching mid-program.
2. Long (24-bit) stores and loads with BOTH widths.
3. `JML` across the program-bank boundary — and proof that K and DB are
   independent registers.
4. `LDA long,X` / `STA long,X` indexed bank-relative access.
5. Hidden-high-byte preservation (`B`) across narrow operations.

## Acceptance criteria

Running your executor on the image must produce ALL of:

- `K == $01` when the program halts (bank crossing happened).
- `$00:$2000 == CD AB` (16-bit long store).
- `$01:$3000 == EF` (8-bit long store).
- `$01:$4007 == EF` (8-bit long,X store with X=$07).
- The instruction trace matches `golden/challenge.trace` line for line.

The committed test `ch31_91_challenge_tests` enforces every criterion.
It must be RED on the skeleton and GREEN once your `step()` (03 block)
is correct — no challenge-specific code is required beyond a working
executor.
