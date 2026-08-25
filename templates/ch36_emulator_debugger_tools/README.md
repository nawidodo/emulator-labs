# ch36_emulator_debugger_tools

Generic `CpuDebug` interface implemented for the fx8 toy machine, trace
logger with filters, instruction history ring, textual VRAM/tile viewers,
an integrated session-transcript challenge, and an observation-based ROM
diagnosis coding test.

## Layout

| Dir | What |
|---|---|
| `01_cpu_debug/` | `dbg::CpuDebug` interface + fx8 implementation: step w/ write report, breakpoints, watchpoints, `regs_json`, disassembler. |
| `02_trace_history/` | Trace logger with opcode/pc/register filters + capped instruction history ring. |
| `03_tile_viewer/` | GB-style 2bpp tile text dump (` .+#`) and CHIP-8 framebuffer viewer. |
| `90_debug/` | Seeded bug in breakpoint matching. Write `bug-report.md`. |
| `91_challenge/` | Scripted debug session → transcript; golden pinned under tests/public. |
| `99_coding_test/` | Diagnose a failing hidden ROM using only debugger commands. |

## Gate checklist

- [ ] exercises RED -> GREEN (`01`, `02`, `03`)
- [ ] debug: `bug-report.md`
- [ ] challenge: transcript matches golden
- [ ] coding_test: hidden manifest passes

## Fixture provenance

Fixtures under `tests/public/ch36_emulator_debugger_tools/` are synthetic;
see `provenance.md`. No commercial ROMs.

## Verification

```
VERIFY_PREFIX=/tmp/labs-ch36 tools/labs/verify_chapter.sh ch36_emulator_debugger_tools
python3 tools/labs/generate.py --mode solution --force --targets ch36_emulator_debugger_tools
cmake -S . -B build-solutions -DLABS_BUILD_SOLUTIONS=On && cmake --build build-solutions -j && \
  ctest --test-dir build-solutions --output-on-failure
python3 tools/labs/grade.py --repo . ch36_emulator_debugger_tools
```
