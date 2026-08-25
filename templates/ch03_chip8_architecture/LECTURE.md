# Chapter 3 — CHIP-8 Machine Architecture (Lecture)

CHIP-8 is technically an interpreted virtual machine rather than physical
hardware, but it is an excellent emulator-development starting point: the whole
machine fits in your head, yet it exercises every muscle you will later need
for a real console — fetch/decode/execute, memory maps, register files,
stacks, timers, and a framebuffer.

## The machine

```text
4096-byte memory
V0–VF          16 8-bit general registers (VF doubles as a flag)
I              16-bit address register
PC             16-bit program counter
stack          16 × 16-bit return addresses + SP
delay timer    8-bit, decremented at 60 Hz when nonzero
sound timer    8-bit, tones while nonzero
64×32 display  monochrome framebuffer, XOR sprite drawing
16-key keypad  4×4 hex keypad (0x0–0xF)
```

### Memory map

| Range        | Contents                                        |
|--------------|-------------------------------------------------|
| `0x000–0x1FF` | reserved for the interpreter                    |
| `0x050–0x09F` | built-in 4×5 font sprites (16 characters)       |
| `0x200–0xFFF` | ROM is loaded here; execution starts at `0x200` |

The `0x200` origin exists because the original interpreters lived in the first
512 bytes of memory. Every emulator you write from now on will have a load
address that is *not* zero — internalize this.

## Instruction encoding

Every CHIP-8 instruction is a **16-bit big-endian** word: the byte at `PC` is
the high byte, the byte at `PC+1` is the low byte. Big-endian fetch means
`op = mem[pc] << 8 | mem[pc+1]` — get this backwards and every opcode decodes
as garbage.

Each instruction packs fields. The canonical field names:

```text
NNN   12-bit address/immediate
NN    8-bit immediate
N     4-bit immediate (often a sprite height)
X     4-bit register selector  -> VX
Y     4-bit register selector  -> VY
```

Example — `6XNN`:

```text
6XNN

0110 XXXX NNNN NNNN
```

For the word `0x6A42`:

| Field | Bits            | Value |
|-------|-----------------|-------|
| op    | `0110`          | 6 → `LD Vx, NN` |
| X     | `1010`          | 0xA → VA |
| NN    | `0100 0010`     | 0x42 |

Field extraction must be pure bit math, done once, in one place:

```text
nnn = op & 0x0FFF
nn  = op & 0x00FF
n   = op & 0x000F
x   = (op >> 8) & 0xF      <- bits 11..8 (low nibble of the high byte)
y   = (op >> 4) & 0xF
```

A classic first bug extracts `x` from the wrong nibble (`op & 0xF`). The
program still runs; registers are simply wrong. You will debug exactly this
bug in exercise 90 using traces, not stares.

## Opcodes implemented in this chapter

| Op     | Mnemonic        | Semantics                                   |
|--------|-----------------|---------------------------------------------|
| `00E0` | `CLS`           | clear the display                            |
| `1NNN` | `JP NNN`        | `PC = NNN`                                   |
| `6XNN` | `LD Vx, NN`     | `VX = NN`                                    |
| `7XNN` | `ADD Vx, NN`    | `VX = (VX + NN) & 0xFF` (8-bit wraparound)   |
| `ANNN` | `LD I, NNN`     | `I = NNN`                                    |
| `DXYN` | `DRW Vx,Vy,N`   | minimal XOR blit used by the challenge (full treatment in chapter 5) |

Unknown opcodes are treated as NOP in this chapter and become hard errors in
chapter 4, which completes the CPU.

## Stepping interface (curriculum §56)

Every emulator core exposes a single-step primitive:

```cpp
struct StepResult {
    uint64_t cycles;   // total executed so far
    uint16_t pc;       // PC after the step
};

StepResult step();
```

This is what makes testing, debugging, tracing, and scheduling possible
without opening a window.

## Headless testing (curriculum §52)

From CHIP-8 onward every emulator is testable without a display:

```bash
./emu --rom tests/cpu.bin --headless --cycles 100000 --trace result.log
```

Our runners all accept the same flags:

```text
--rom PATH --headless --cycles N --frames N --trace FILE --hash-frame FILE
```

## Trace format

All chapter runners emit one line per step, whitespace-separated
`key=value` tokens, lowercase keys:

```text
pc=0200 op=00E0 V0=00 V1=00 ... VF=00 I=000 SP=00 DT=00 ST=00 cyc=1
```

- `pc` — PC **after** executing the step (post-state, so jumps are visible)
- `op` — the fetched opcode (pre-execution snapshot of the instruction)
- `V0..VF`, `I`, `SP`, `DT`, `ST` — register state **after** the step
- `cyc` — total cycles executed after the step

Golden traces live under `tests/public/ch03_chip8_architecture/traces/`.
Compare two traces with `tools/labs/compare_trace.py` and hunt the **first
divergence**, not the last visible symptom (§54).

## Every CPU needs a disassembler (§55)

You are not allowed to wait for the debugger chapter. The mandatory interface:

```cpp
std::string disassemble(uint16_t pc);
```

This chapter's format: `ADDR: OPCODE  MNEMONIC`, e.g.
`0202: A224  LD I, 0x224`.

## Every device is independently testable (§57)

`Chip8` is a self-contained class: tests instantiate it, poke memory or
registers through small test hooks, call `step()`, and assert on state. No
window, no threads, no host paths in assertions — deterministic only.

## Study references

- Tobias Langhoff, "CHIP-8 architecture" guide
- Timendus, "CHIP-8 test suite" (optional, gated as `requires_rom`)
