# Chapter 2 — Building an Emulator Core

Read `SPEC.md` first: it defines LAB-8, the fictional CPU every exercise in
this chapter implements.

## The universal loop

Every interpreted emulator — CHIP-8, 8080, Game Boy, NES, PS1 — is the same
three-line loop:

```cpp
while (running) {
    auto insn = fetch();    // read raw bytes at pc, advance pc
    auto dec  = decode(insn); // opcode -> meaning + operands
    execute(dec);           // mutate machine state
}
```

FETCH → DECODE → EXECUTE → UPDATE STATE → NEXT INSTRUCTION.

The loop is universal because hardware is universal: a real CPU fetches from
a bus, decodes combinatorially, executes in a datapath, and repeats. An
emulator is that datapath expressed as data structures. Everything else in
this course — interrupts, DMA, pipelines, MMIO — is elaboration on where
bytes come from and what execute() touches, never a change to the loop's
shape.

Two properties the loop must have from day one:

1. **It advances through one indivisible unit of work**: one instruction per
   `step()`, returning what happened (`StepResult{cycles, pc, error}`).
   Tests, tracing, frame scheduling, and later debugger breakpoints all hang
   off this single function. Never expose a `run_forever()` without a
   step-sized primitive underneath.
2. **It is deterministic**: same ROM + same inputs → byte-identical state and
   traces. No wall-clock time, no uninitialized reads, no host-dependent
   arithmetic. Determinism is what makes golden-trace comparison possible.

## State structs

Model the machine as plain data first:

```cpp
struct Cpu {
    uint8_t r[4];
    uint8_t ram[256];
    uint8_t pc;
};
```

Guidelines that stay true for 300-register ARM:

- Registers are fixed-size arrays or scalars with defined reset values.
  Zero-initialize everything; an uninitialized register is a nondeterminism
  bug wearing a disguise.
- Keep *architectural* state (visible to programs: registers, RAM, PC,
  flags) separate from *implementation* state (caches, cycle accumulators).
  Serialization, save states, and trace comparison care only about the former.
- Prefer value semantics and small structs you can print wholesale. If you
  cannot dump the whole CPU state in one line, tracing will hurt later.

## Fetch

Fetch reads the *raw encoding* and nothing else: how many operand bytes does
this opcode take, gather them (honoring PC wraparound), advance pc past the
instruction. Lengths come from a table keyed by opcode — never scattered
`if` chains. Note what fetch does NOT do: it does not interpret bits, does
not touch flags, does not validate operands.

A useful discipline: after fetch, pc always points at the next instruction.
Execute may then overwrite pc freely for jumps without bookkeeping regrets.
(The seeded bugs in `90_debug/` both live exactly at this seam.)

## Decode tables vs switch

You will meet two idioms for decode:

```cpp
// (a) switch — fine below ~64 opcodes, compiler builds the jump table for you
switch (opcode) {
    case 0x40: return {Op::Add, b0, b1};
    ...
}

// (b) table — scales, data-driven, introspectable
constexpr InsnInfo kTable[256] = {
    [0x00] = {Op::Halt, .len = 1, .cycles = 4},
    [0x10] = {Op::Load, .len = 3, .cycles = 4},
    ...
};
```

Rules of thumb:

- Small ISA → switch. The compiler emits a jump table anyway, and the case
  labels document the opcode map inline.
- Large or irregular ISA (prefix bytes, per-opcode addressing modes) → table,
  because the metadata (length, cycles, mnemonic, valid modes) becomes data
  you can reuse for disassembly, tracing, and debuggers.
- Either way, decode validates: reserved encodings become explicit errors
  (`UnknownOpcode`, `BadRegister`), never silent modulo tricks. Real cores
  trap or UNDEF; emulators that "gracefully" mask reserved fields diverge
  from hardware and hide ROM bugs.

## Function dispatch

The chapter solution deliberately separates fetch / decode / execute into
functions instead of one giant switch inside `step()`:

```cpp
StepResult Cpu::step() {
    RawInsn raw = fetch();
    Decoded  d   = decode(raw);
    StepResult r{};
    execute(d, r);
    return r;
}
```

Why it matters once your ISA grows past a page:

- Each handler is unit-testable alone (`TEST(Alu, AddSetsCarry)`).
- Cycle accounting lives in one place instead of being smeared across cases.
- Handlers group naturally by subsystem (ALU, memory, flow), which is how
  real cores organize their micro-ops.
- A giant switch grows quadratically in review effort; handlers grow linearly.

For big ISAs you upgrade dispatch to a table of member-function pointers
(`using Handler = void (Cpu::*)(const Decoded&)`) — same shape, indexed by
decode output. That is the bridge to threaded/dispatched interpreter cores in
the advanced chapters.

## Errors

Undefined opcodes, bad register fields, stack overflow (Chapter challenge):
these are *machine-visible events*, not exceptions to swallow. Contract used
throughout this course:

- `step()` returns an error code in its result; it never throws and never
  calls exit().
- On error, the instruction's effects do NOT happen, but the cost of noticing
  the problem (fetch) does — deterministic, documented in SPEC.md.
- The machine halts rather than guessing. Silent wrong execution is the one
  unforgivable emulator bug: it corrupts traces, goldens, and your sanity.

## Tracing

One canonical line per executed instruction:

```
pc=07 op=50 r0=06 r1=01 r2=00 r3=00 cyc=4
```

Design rules:

- Emit BEFORE the instruction's effects: `pc`/`op` name what was fetched;
  registers show pre-state; `cyc` reports what the instruction cost. When
  two implementations diverge, the first differing line tells you which
  instruction misbehaved and what state it saw — trace-first debugging.
- Whitespace-separated `key=value`, lowercase keys, no timestamps. Machine-
  diffable (`tools/labs/compare_trace.py` aligns lines by index and reports
  the first divergence with context).
- Tracing must be free when off (null sink) and must not change behavior when
  on. A tracer that perturbs the machine it observes is useless for
  reproducibility.

```bash
./ch02_04_runner --rom prog.bin --cycles 500 --trace actual.log
python3 tools/labs/compare_trace.py golden.log actual.log
```

## Reproducibility

Golden hashes and traces are only meaningful if execution is deterministic:

- Integer guest clocks everywhere; cycles are counted, never measured.
- No RNG without a fixed seed (LAB-8 has none — keep it that way).
- Every state byte initialized before use.
- Goldens are generated by running the reference solution twice and checking
  the runs agree (see `tests/public/ch02_emulator_core/traces/provenance.md`).

This is the contract that lets hidden tests compare a single FNV-1a hash of
your trace against the reference implementation's, across machines.
