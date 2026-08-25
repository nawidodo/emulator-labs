# LECTURE — Emulator Laboratory Infrastructure

Everything in this course is built on a small set of primitives: reading
bytes with the right byte order, slicing fields out of words, looking at
binary data, and turning annotated reference code into graded student
skeletons. This chapter builds and dogfoods all of them.

## 1. Emulator, simulator, interpreter

| term        | meaning |
|-------------|---------|
| emulator    | reproduces the *behavior* of one machine on another, hardware quirks included |
| simulator   | models a system's *logic* at an chosen level of fidelity (often higher-level than gates, lower than behavior) |
| interpreter | executes a program's *instructions* directly — an emulator's CPU core is an interpreter of the guest ISA |

We build emulators whose CPU cores are interpreters first. Dynamic
recompilation (JIT) is a later optimization; correctness comes from the
interpreter.

## 2. Host versus guest

- **Guest**: the machine being emulated — its ISA, memory map, timing.
- **Host**: the machine running your emulator.

Every line you write must know which side it is on: a `uint16_t` opcode
fetch is a *guest* concept; the `uint8_t[]` array backing it is a *host*
implementation detail. Endianness bugs are exactly this boundary failing:
the host reads guest bytes in host order.

## 3. Deterministic execution

An emulator is deterministic when identical initial state + identical
inputs ⇒ identical state traces. Determinism is what makes

- golden-hash tests,
- trace comparison,
- save-state debugging,

possible at all. Rules we enforce everywhere in this repo: no wall-clock
time in cores, no uninitialized RNG, no iteration over unordered
containers where order is observable, fixed-point arithmetic for timing.
The skeleton generator itself obeys the same law — running it twice must
produce byte-identical trees, and the hidden tests hold you to that.

## 4. Binary files, hexadecimal, bit operations

A ROM/cartridge/ISO is just bytes; structure exists only because the
documentation says so. You need three mechanical skills until they are
reflexes:

1. **Multi-byte assembly** — `read_le16/be16/le32`: which byte carries
   which significance. Little-endian = low byte first ("little end
   first"). x86, Game Boy, NES, GBA data are little-endian; CHIP-8
   opcodes and network order are big-endian.
2. **Bit-field extraction** — `bits(value, start, count)`:
   `(value >> start) & mask`. Opcode decoding is hundreds of these;
   make the idiom automatic, including the `count == 32` edge case where
   `1 << 32` would be undefined.
3. **Hex literacy** — read dumps by eye: offsets, column discipline, the
   ASCII gutter. The dumper you build in exercise 02 is the microscope
   used in every later chapter.

## 5. Cycles and machine state

- **Machine state**: the complete observable condition of the guest —
  registers, RAM, pending interrupts, timer counters, video memory.
  Save states are serializations of exactly this.
- **Cycles**: the guest's currency of time. Each instruction costs a
  documented number of cycles; emulators step either per instruction or
  per cycle (`step()` vs `run(cycles)`). The headless runner flag
  `--cycles N` caps simulated work without any display.

## 6. Test-driven emulation

Write the test before the hardware:

```
fixture bytes -> run core -> compare trace / frame hash -> GREEN
```

Red skeletons, golden hashes, and trace diffs all instantiate this loop.
If you cannot express expected behavior as bytes-in/hash-out, you do not
understand the hardware yet — which is the point of writing the test
first.

## 7. Headless cores, frontend vs core

The emulator core never depends on the GUI:

```text
Frontend (window, audio out, input polling)
   │
   ↓
Emulator core (headless, deterministic)
 ├── CPU      instruction interpreter + cycles
 ├── Bus      address decode, memory map, MMIO
 ├── Video    PPU/GPU -> framebuffer
 ├── Audio    APU/SPU -> sample stream
 └── Input    polled by the core through a narrow interface
```

The frontend feeds ROMs and scripted input, steps the core, and consumes
frames/samples. Because the core is headless, `--headless --frames N
--hash-frame FILE` works in CI with no window server — every chapter's
runner exposes exactly those flags.

## What you do this chapter

| exercise | skill |
|---|---|
| 01_endian_bits | multi-byte assembly + bit extraction |
| 02_hex_dumper | binary inspection tooling |
| 03_generator_starter | the lab infrastructure itself |
| 90_debug | diagnosing endian bugs from symptoms |
| 91_challenge | multi-target generation + hash verification |
| 99_coding_test | apply your generator to an unseen template |
