# 03 — nestest-style Trace Logging

Reference emulators earn trust by producing logs you can diff line-by-line
against known-good hardware traces. The canonical reference is nestest's
log. This exercise builds the same observability for the course core.

## Tasks

1. **`disassemble_at(bus, pc)`** — render `MNEMONIC [operand]` for a PEEKED
   instruction (no execution, no cycle billing). All eleven operand shapes,
   unofficial mnemonics included, undecoded rows as `???`.
2. **`disasm_len(op)` / `peek_trace(bus, pc)`** — byte length per mode
   family and a pre-step snapshot (`TraceRow`) so operand bytes are read
   before the instruction can rewrite them.
3. **`trace_line(cpu, row)`** — assemble the finished column-format line:

   ```
   0600  A9 05     LDA #$05   A:05 X:00 Y:00 P:22 SP:FD CYC:7
   ```

   PC as `%04X`; three byte slots (`XX `, blanks when absent); disassembly
   left-justified in 10 columns; post-step registers; cumulative CYC.
   (Course variant of the nestest log: no PPU column — our flat-RAM rig
   has no PPU.)

## Acceptance

- All `trace.*` tests pass, including exact formatted lines and lengths
  for every addressing family.
- The runner supports both `--trace` (canonical key=value) and
  `--trace-log` (columns above).
