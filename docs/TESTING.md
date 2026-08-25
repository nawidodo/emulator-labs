# Testing Philosophy

## Headless first

From CHIP-8 onward every emulator is testable without opening a window:

```bash
./emu --rom tests/cpu.bin --headless --cycles 100000 --trace result.log
python3 tools/labs/compare_trace.py expected.log result.log
```

Frame equality is a hash, never an eyeball:

```bash
./emu --rom game.bin --frames 60 --hash-frame f.rgba
python3 tools/labs/hash_frame.py f.rgba --fnv-only
```

## The test pyramid

```
        Game tests          (milestone fixtures)
      Hardware ROM tests    (course-original suites; external suites optional)
    Component tests         (CPU/PPU/DMA/APU in isolation)
  Unit tests                (ALU/decode/field extraction)
```

Every chapter wires its layers through CTest; hidden layers via `make grade`.

## Trace-first debugging

When behavior diverges: capture both traces, diff, find the FIRST divergence.
`tools/labs/compare_trace.py` reports it with context and can ignore fields
(e.g. `--ignore cyc`) when comparing against a reference with coarser cycle
accounting.

## House rules (curriculum §55-58)

1. Every CPU gets a disassembler early (`std::string disassemble(pc)`), not
   in some later debugger chapter.
2. Every core exposes `StepResult step()` — testing, tracing and scheduling
   all hang off it.
3. Every device instantiates alone: `Gpu gpu;` without building a whole PS1.
4. Every hardware register gets a specification test
   (`TEST(DmaChannel, StartsWhenEnableAndTriggerSet)`).
5. **Never invent hardware values.** Unimplemented registers, unmapped
   ranges and open buses must be LOGGED/trapped deterministically (fixed
   sentinel or trace event), never silently fabricated. Silent invention
   turns "unknown" into an untestable lie.

## Determinism or it didn't happen

Golden hashes are only meaningful if execution is deterministic: integer guest
clocks everywhere, no wall time, seeded RNG only, no uninitialized state in
serialized data. If two runs of YOUR solution disagree on a hash, that's a bug
to fix before the golden means anything.
