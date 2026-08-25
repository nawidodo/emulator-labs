# Challenge — Run a Real 8080 Program

Chapter 7 has no control flow yet, so the challenge fixture is a
**straight-line diagnostic**: seventeen instructions that walk the full
arithmetic/logic matrix (`ADD/ADI`, `SUI/ACI/SBB`, `ANI/XRI/ORA`) through
register, immediate and direct-memory addressing, storing five result bytes
into a table at `0x4000` before halting.

## Why this is the acceptance test

Every stored byte and every final flag encodes an exact hardware behavior:

| Address | Value | Proves |
|---|---|---|
| 4000 | 66 | ADD carry-free path |
| 4001 | 30 | AND + the 8080 AC quirk (bit-3 OR of operands) |
| 4002 | FF | XOR clears CY/AC |
| 4003 | FF | SBB borrow chain (A - A - CY) |
| 4004 | 66 | ORA A flag recompute after LDA |

The run must also halt cleanly with **exactly 156 T-states** consumed.
Wrong cycle accounting anywhere (MOV 5T vs memory 7T, ALU 4T vs immediate
7T, LDA/STA 13T) shifts the total.

## Acceptance criteria

```bash
ctest --test-dir build -R ch07_91_challenge   # all four cases green
```

and, against the committed public fixture:

```bash
./build/skels/ch07_i8080_architecture/03_cpu_core/i8080_runner \
    --rom tests/public/ch07_i8080_architecture/roms/ch07_diag.bin \
    --cycles 1000
# AF=66 02 ... cyc=156
```

## Going further

External reference suites are gated behind student-supplied ROMs and stay
optional: see the `requires_rom` entries in the chapter's hidden manifest.
Useful pointers:

- 8080 instruction set timing tables (Intel 8080 Assembly Language
  Programming Manual, Rev. B)
- TST8080.COM / CPUTEST.COM diagnostics from the classic 8080 exorciser
  suite (student-supplied; never committed here)
