# Chapter 3 — CHIP-8 Machine Architecture

CHIP-8 is the first *real* machine of the course: 4096 bytes of memory,
sixteen registers, a stack, timers, a 64x32 framebuffer and 16 keys. This
chapter builds the machine skeleton, fetch/decode plumbing, the five base
instructions, the mandatory disassembler (§55), and ends with the classic
logo-rendering challenge plus an unseen-spec coding test.

Read `LECTURE.md` first; it carries the encoding-field tables
(`NNN NN N X Y`), the headless-testing contract (§52), the trace format, and
the stepping interface (§56).

## Exercises

| Dir | Task | Gate |
|-----|------|------|
| `01_state_and_load` | machine state + `reset()` (fontset at 0x050, PC=0x200) + `load()` at 0x200 | exercises |
| `02_fetch_decode` | big-endian opcode fetch + pure field extraction `nnn/nn/n/x/y`, exhaustively unit-tested | exercises |
| `03_base_ops` | `00E0 CLS`, `1NNN JP`, `6XNN LD Vx`, `7XNN ADD Vx`, `ANNN LD I` driven headlessly from tests | exercises |
| `04_disassembler` | `disassemble(pc)` table-driven formatting tests + ROM listing runner | exercises |
| `05_ibm_logo` | minimal `DXYN` blit + challenge: render the logo frame, golden FNV64 must match | challenge |
| `90_debug` | seeded bug: decoder takes register X from the wrong nibble; find it via traces, ship `bug-report.md` | debug |
| `99_coding_test` | implement `2NNN CALL`, `00EE RET`, `3XNN SE`, `4XNN SNE`, `BNNN JP V0+NNN` from spec alone | coding_test |

## Fixtures

All programs are course-original, hand-assembled byte arrays (no commercial
or third-party ROM bytes):

- `tests/public/ch03_chip8_architecture/roms/` — `base_demo.ch8`,
  `fetch_demo.ch8`, `ibm_logo.ch8`, each with an annotated `.asm.txt`.
- `tests/public/ch03_chip8_architecture/traces/` — golden traces produced by
  the reference solution (see `provenance.md` for generating commands).
- `tests/hidden/ch03_chip8_architecture/` — grader manifest + crafted ROM
  images for the coding test.

External hardware suites remain optional URL references gated by
`requires_rom` in the hidden manifest (Timendus CHIP-8 test suite).

Authored and verified with the isolated harness (2026-08-25):

```bash
VERIFY_PREFIX=/tmp/labs-Ch03Chip8Arch tools/labs/verify_chapter.sh ch03_chip8_architecture
# verdict: skel_build=ok solutions=GREEN
```

- Skeleton tree builds; 6 of 13 ctest entries RED as designed
  (`ch03_01_state`, `ch03_02_fetch`, `ch03_03_ops`, `ch03_04_disasm`,
  `ch03_05_logo`, `ch03_90_debug`) — one red test entry per TODO exercise;
  runner `--help` smoke tests stay green.
- Solution tree: 13/13 GREEN.
- All golden traces/frame digests generated twice from the reference
  solution with byte-identical results (`tests/public/.../provenance.md`).
- Hidden manifest validated by executing the built scratch binaries with
  the exact manifest args: 10/10 non-optional cases PASS on solutions,
  FAIL on skeletons; Timendus case SKIPs without the student-supplied ROM.

