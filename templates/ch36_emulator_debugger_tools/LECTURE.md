# Lecture 36 — Emulator Debugger and Developer Tools

Professional emulator development is tooling-driven. When a game breaks,
you do not stare at code — you open the debugger on your own emulator and
look at what the *guest* is doing.

## The generic CPU-debug interface

One interface, many machines — the same shape works for the toy fx8, a
6502, or an ARM7TDMI:

```cpp
class CpuDebug {
    StepInfo step();                       // execute ONE instruction
    std::string regs_json() const;         // register viewer payload
    std::string disassemble(addr_t pc);    // curriculum §55 mandate
    uint8_t  read_mem(addr_t);             // memory viewer
    void     write_mem(addr_t, v);
    void add_breakpoint(addr_t);           // stop when PC == addr
    void add_watchpoint(addr_t);           // stop when *addr changes
};
```

`step()` returning `{pc_before, opcode, cycles, writes[]}` is the atom
everything else is built from: tracer, history, coverage, profilers.

## Disassembler rules (§55)

Format: `addr: opcode bytes  MNEMONIC operand`. One line per instruction,
pure function of memory — never of machine state. You were required to
have one since the first CPU chapter; here it becomes load-bearing.

## Breakpoints vs watchpoints

- **Breakpoint** — fire on control flow: `pc == target`. Count hits even
  when not stopping; hit counts turn "is this path hot?" into one query.
- **Watchpoint** — fire on data flow: the value at an address changed.
  Implementation trick for simple interpreters: snapshot memory before
  the step, diff after. Real emulators hook bus writes instead (page
  guards / MMU traps) because full snapshots are too slow — same
  observable semantics, different cost model.

Watchpoints find what breaks; breakpoints find where. A corrupted VRAM
byte is answered in seconds by "watch that address", while printf-hunting
the writer takes hours.

## Trace logger with filters

A full trace is megabytes of noise. Filters keep it useful:

- by opcode (`op >= 0x04 && op <= 0x06` — all arithmetic),
- by PC region (one function),
- by register condition (`a == 0`),
- post-mortem mode: keep everything in a ring, dump only AFTER failure —
  the last 4096 instructions before the crash are the ones you want.

Trace-first debugging (§54): run reference emulator + yours, diff traces,
fix the FIRST divergence — never the last visible symptom.

## Instruction history ring

Same data as the trace, but always resident and capped: `push` overwrites
oldest. This powers "what happened just before the breakpoint?" — the
question you ask within five seconds of every halt.

## VRAM / tile viewers

Graphics bugs are memory-layout bugs: wrong tile index, swapped bit
planes, off-by-eight scroll. A **textual** tile viewer renders each 2bpp
tile as 8×8 characters (` .+#` for values 0–3):

```text
tile $12 @ vram+0x0240
+##+ . .
#..# . .
#### . .
#..# . .
```

No GUI needed — it diffs cleanly in golden tests and works over SSH.
Every console debugger you will ever love started as exactly this kind of
text dump.

## What you build here

- `01_cpu_debug` — the `CpuDebug` interface implemented for the fx8:
  step-with-write-report, breakpoints, watchpoints, JSON registers,
  disassembler.
- `02_trace_history` — filtered trace logger + instruction history ring.
- `03_tile_viewer` — GB-style 2bpp tile dump and CHIP-8-style framebuffer
  viewer, pure text.
- `90_debug` — a seeded defect in breakpoint matching.
- `91_challenge` — integrated debug session producing a transcript
  fixture (scripted commands → pinned output).
- `99_coding_test` — diagnose a failing ROM using ONLY debugger
  commands; the graded artifact is a diagnosis token derived from live
  observation, so a broken emulator cannot fake it.
