# Coding Test — ch11: Unseen Opcode Family ($FF00-page access)

You get a written specification and an empty hook. Implement the five
opcodes below in `ldh.hpp`, install them with `gb::install_ldh_hook(cpu)`
(a chain `Cpu::Hook`, exactly like `IrqHook`), and make the visible suite —
plus the hidden probe cases — pass.

The ch11 core decoder claims none of these rows today; every one currently
traps.

## Specification

All register forms live in the high window `$FF00-$FFFF`:

| Opcode | Syntax | Semantics | Cycles |
|---|---|---|---|
| `E0 nn` | `ldh (n),A`   | write A to `$FF00+nn`       | 12 |
| `F0 nn` | `ldh A,(n)`   | read `$FF00+nn` into A      | 12 |
| `E2`    | `ldh (C),A`   | write A to `$FF00+C`        | 12 |
| `F2`    | `ldh A,(C)`   | read `$FF00+C` into A       | 12 |
| `08 nn nn` | `ld (nn),SP` | store SP at nn, low byte first | 20 |

Contract details that matter:

- At this model level the four register forms cost a uniform **12 T-cycles**
  (operand fetch + internal + memory M-cycle). The `(C)` forms have no
  operand byte but keep the same price here.
- `LD (nn),SP` costs **20** (5 M-cycles) and writes SP little-endian.
- **No family member touches any flag.** F must survive byte-for-byte.
- The dispatcher returns `false` for every other opcode so the rest of the
  hook chain (IRQ, stack, DAA/CB, core) still sees them.

## Methodology

Trace first, against the public challenge goldens: build the 99 runner and
run a program you assemble yourself, then diff your instruction trace line
by line against what the spec predicts (`pc/op/af/bc/de/hl/sp/cyc`). The
FIRST divergence — wrong address, wrong A, wrong cycle count — tells you
which half of the row is broken. The public smoke fixtures
(`tests/public/ch11_gameboy_cpu_completion/fixtures/`) show the expected
trace shape.

## Grading

Hidden cases run your implementation through `ch11_99_coding_test_runner`
over an unseen probe binary (`ldh_probe.bin`) that exercises all five rows
with observable HRAM/WRAM effects; grading checks the run's exit status,
stdout summary, and a golden hash over the final-state dump. Visible tests
only cover the easy half on purpose.
