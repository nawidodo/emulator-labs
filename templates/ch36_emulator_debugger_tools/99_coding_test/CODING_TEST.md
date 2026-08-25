# Coding test — diagnose a failing ROM using only debugger tools (ch36)

You are handed an fx8 program that HANGS on real behavior expectations.
Using ONLY your debugger (step/disasm/breakpoints/mem), you must produce
a machine-derived diagnosis token. A broken emulator produces a broken
token — the grade cannot be faked.

## The diagnostic protocol (fixed)

1. Run at most 32 instructions through `CpuDebug::step()`, counting PC
   visits.
2. The FIRST pc to reach 3 visits is `loop_pc` — stop immediately.
3. Emit ONE line to `--result FILE`:

```text
diagnosis=infinite_loop loop_pc=<hex2> insn="<disasm(loop_pc)>" mem20=<hex2>
```

`mem20` is the value at address $20 at the moment the loop was detected.
If no loop is found within 32 steps (or the CPU halts), emit
`diagnosis=clean` instead and exit 0.

Exit codes: 0 with a result line; 2 if the debugger pieces are not
implemented yet.

## Why observation-based grading

The token hashes live machine observations (pc visit counts, memory
content). Any defect in stepping, disassembly, or write tracking changes
the token — so passing this test certifies your whole toolchain, not
just that the program exits.
