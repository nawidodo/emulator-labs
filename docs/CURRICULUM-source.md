# Zero-to-Expert Game Emulator Engineering

## Goal

The target is not:

> “I followed a tutorial and got Pong running.”

The target is:

```text
ROM / executable
      ↓
loader
      ↓
CPU
      ↓
bus / memory map
      ↓
timers / interrupts / DMA
      ↓
PPU / GPU
      ↓
APU / SPU
      ↓
input / storage / peripherals
      ↓
scheduler
      ↓
accurate emulated machine
```

By graduation, you should be able to receive hardware documentation for an unfamiliar game system and independently:

- design an emulator architecture
- implement an instruction-set interpreter
- decode binary instructions
- implement memory maps
- model memory-mapped I/O
- emulate timers and interrupts
- implement DMA
- emulate raster graphics
- emulate audio hardware
- synchronize multiple devices
- build cartridge/disc loaders
- implement save states
- build a debugger
- create trace-based differential tests
- use hardware test ROMs
- diagnose timing bugs
- optimize hot paths
- understand interpreter versus JIT/dynamic recompilation
- implement a PS1-class emulator
- work from specifications instead of tutorials

The main implementation language will be:

```text
C++20
```

with:

```text
CMake
CTest
SDL3 or another thin platform frontend
```

The **emulator core must never depend directly on the GUI**.

---

# 1. The Learning Path

The main progression is:

```text
Foundations
    ↓
CHIP-8
    ↓
Intel 8080 / Space Invaders
    ↓
Game Boy
    ↓
NES
    ↓
Game Boy Advance
    ↓
SNES
    ↓
Advanced Emulator Architecture
    ↓
PlayStation 1
```

Each new system introduces additional complexity.

| System | New concepts |
|---|---|
| CHIP-8 | fetch/decode/execute |
| 8080 | real CPU ISA |
| Space Invaders | interrupts + hardware I/O |
| Game Boy | bus + timers + scanline PPU |
| NES | CPU/PPU concurrency + mappers |
| GBA | ARM + pipelines + DMA |
| SNES | multiple processors + HDMA + complex PPU |
| PS1 | 32-bit RISC + DMA fabric + GPU + GTE + CD + SPU |

CHIP-8 is a particularly good first implementation because its machine model is tiny: roughly 4 KiB memory, a small register set, stack, timers, keypad, and monochrome display.

---

# 2. Linux Kernel Labs-Style Infrastructure

Every chapter follows the laboratory infrastructure you referenced.

Linux Kernel Labs structures learning around incremental hands-on tasks using generated skeleton code. Full implementations live under templates, while student versions contain explicit `TODO`, `TODO1`, `TODO2`, etc. Tasks can be generated individually using `LABS`, and later TODO checkpoints can be generated using the `TODO` variable.

We will reproduce that design.

```text
emulator-labs/
│
├── README.md
├── Makefile
├── progress.json
│
├── tools/
│   └── labs/
│       ├── generate.py
│       ├── common.mk
│       ├── grade.py
│       ├── compare_trace.py
│       └── hash_frame.py
│
├── templates/
│   ├── ch01/
│   ├── ch02/
│   └── ...
│
├── skels/
│
├── solutions/
│
├── tests/
│   ├── public/
│   └── hidden/
│
├── roms/
│   └── homebrew-tests/
│
├── traces/
│
├── docs/
│
└── third_party/
```

---

# 3. Skeleton Workflow

Generate one chapter:

```bash
make clean
LABS=ch03_chip8_cpu make skels
```

Generate one exercise:

```bash
LABS=ch03_chip8_cpu/04_draw make skels
```

Resume from TODO 4:

```bash
TODO=4 LABS=ch03_chip8_cpu/04_draw make skels
```

Build:

```bash
make build
```

Test:

```bash
make test
```

Debug:

```bash
make debug
```

Sanitizers:

```bash
make sanitize
```

Run hidden coding-test cases:

```bash
make grade
```

Run trace comparison:

```bash
make trace-test
```

This mirrors the generate → modify → build → observe workflow used by Linux Kernel Labs.

---

# 4. Chapter Gate

Every chapter contains:

```text
LECTURE
   ↓
SOURCE / SPEC EXPLORATION
   ↓
EXERCISES
   ↓
STARTER PROJECT
   ↓
DEBUGGING EXERCISE
   ↓
CHALLENGE
   ↓
CODE TEST
   ↓
REVIEW
   ↓
PASS
```

To unlock Chapter N+1:

```text
all exercises       PASS
starter             PASS
debugging exercise  PASS
challenge           PASS
coding test         PASS
```

Anything incomplete means:

```text
NEXT CHAPTER

🔒 LOCKED
```

There is **no theory quiz that can compensate for broken code**.

---

# 5. Solution Policy

Every exercise has a full solution in:

```text
templates/
```

and eventually:

```text
solutions/
```

But it remains unavailable while you are actively solving the exercise.

Hint escalation:

```text
Hint 0
↓
concept involved

Hint 1
↓
relevant structure/function

Hint 2
↓
relevant algorithm

Hint 3
↓
pseudocode

Hint 4
↓
partial implementation

Give up
↓
complete reference solution
```

If you request the full solution, you can still study it, but that particular task is marked:

```text
SOLVED WITH REFERENCE
```

and you must pass a new coding test covering the same concept before progressing.

---

# 6. ROM Policy

Labs primarily use:

```text
homebrew ROMs
test ROMs
programs we write ourselves
public-domain samples
your own legally obtained dumps
```

Commercial ROM images are not part of the repository.

---

# PHASE I — EMULATOR FOUNDATIONS

# Chapter 1 — Emulator Laboratory Infrastructure

## Lecture

Learn:

- emulator versus simulator versus interpreter
- host versus guest
- deterministic execution
- binary files
- hexadecimal
- bit operations
- endianness
- cycles
- machine state
- test-driven emulation
- headless cores

Architecture:

```text
Frontend
   │
   ↓
Emulator
 ├── CPU
 ├── Bus
 ├── Video
 ├── Audio
 └── Input
```

## Exercises

### Exercise 1.1

Implement:

```cpp
uint16_t read_le16(const uint8_t* p);
uint16_t read_be16(const uint8_t* p);
uint32_t read_le32(const uint8_t* p);
```

### Exercise 1.2

Implement bit extraction:

```cpp
uint32_t bits(
    uint32_t value,
    unsigned start,
    unsigned count);
```

### Exercise 1.3

Build a binary hex dumper.

### Exercise 1.4

Implement the Linux-Kernel-Labs-style skeleton generator.

## Starter

```text
tools/labs/generate.py
```

TODO:

```text
TODO1 discover template
TODO2 copy files
TODO3 remove completed blocks
TODO4 preserve target TODO
TODO5 generate manifest
```

## Debugging exercise

Fix intentionally broken endian decoding.

## Challenge

Support:

```bash
LABS="ch01/a ch01/b ch01/c" make skels
```

## Solution

Reference solution implements:

```text
template discovery
TODO parser
manifest generation
incremental skeleton generation
```

## Coding test

Given a new template with seven TODO levels, generate every valid skeleton version.

## Gate

All output hashes must match.

---

# Chapter 2 — Building an Emulator Core

## Lecture

Learn the universal loop:

```cpp
while (running) {
    opcode = fetch();
    instruction = decode(opcode);
    execute(instruction);
}
```

And:

```text
FETCH
 ↓
DECODE
 ↓
EXECUTE
 ↓
UPDATE MACHINE STATE
 ↓
NEXT INSTRUCTION
```

Learn:

- state structs
- decode tables
- function dispatch
- errors
- tracing
- reproducibility

## Exercises

Implement a fictional CPU:

```text
4 registers
256 bytes RAM
8 instructions
```

Instructions:

```text
LOAD
ADD
SUB
JMP
JZ
STORE
LOADM
HALT
```

## Starter

```cpp
struct Cpu {
    uint8_t r[4];
    uint8_t ram[256];
    uint8_t pc;
};
```

## Challenge

Add:

```text
CALL
RET
stack
flags
```

## Solution

Reference implementation separates:

```text
fetch
decode
execute
```

instead of one giant switch.

## Coding test

Implement an unseen 12-instruction fictional architecture.

---

# PHASE II — CHIP-8

# Chapter 3 — CHIP-8 Machine Architecture

CHIP-8 is technically an interpreted virtual machine rather than physical hardware, but it is an excellent emulator-development starting point.

## Lecture

Study:

```text
4096-byte memory
V0–VF
I
PC
stack
delay timer
sound timer
64×32 display
16-key keypad
```

## Source/spec exploration

Identify instruction encoding fields:

```text
NNN
NN
N
X
Y
```

Example:

```text
6XNN

0110 XXXX NNNN NNNN
```

## Exercises

### TODO1

ROM loading at:

```text
0x200
```

### TODO2

Fetch 16-bit opcode.

### TODO3

Decode X/Y/N/NN/NNN.

### TODO4

Implement:

```text
00E0
1NNN
6XNN
7XNN
ANNN
```

## Starter

```cpp
class Chip8 {
public:
    void reset();
    void load(std::span<const uint8_t>);
    void step();

private:
    std::array<uint8_t, 4096> memory_;
};
```

## Debugging exercise

A broken decoder extracts `X` from the wrong nibble.

Find it using trace output.

## Challenge

Make the CHIP-8 IBM-logo test render correctly.

## Solution

Reference implementation includes clean opcode-field extraction.

## Coding test

Implement five unseen CHIP-8 instructions from specification alone.

---

# Chapter 4 — Complete CHIP-8 CPU

## Lecture

Study all instruction families:

```text
jumps
calls
conditionals
arithmetic
logic
shifts
random
memory operations
BCD
key operations
```

## Exercises

Implement instruction groups incrementally:

```text
TODO1 control flow
TODO2 ALU
TODO3 carry/borrow
TODO4 shifts
TODO5 memory
TODO6 BCD
```

## Starter

Partially complete CPU dispatch table.

## Debugging exercise

Find four intentionally incorrect flag behaviors.

## Challenge

Pass the Corax+ and flags tests from the CHIP-8 test suite. That suite includes opcode, flags, quirks, keypad, beep, and display tests specifically for emulator development.

## Solution

Reference implementation includes explicit quirk configuration rather than scattered special cases.

## Coding test

Implement an unseen instruction-set extension based only on a short specification.

---

# Chapter 5 — CHIP-8 Graphics, Input and Timers

## Lecture

Learn:

```text
framebuffers
XOR sprites
collision flag
wrapping/clipping
key state
60 Hz timers
host timing
```

## Exercises

### TODO1

Framebuffer:

```cpp
bool display[64 * 32];
```

### TODO2

Implement `DXYN`.

### TODO3

Implement keypad.

### TODO4

Implement timers.

### TODO5

Separate CPU rate from timer rate.

## Starter

Headless framebuffer backend.

## Challenge

Pass:

```text
CHIP-8 display test
keypad test
beep test
```

## Solution

Reference implementation uses separate timing accumulators.

## Coding test

Implement deterministic execution for exactly N CPU cycles and M timer ticks.

---

# Chapter 6 — CHIP-8 Accuracy, Quirks and Debugger

## Lecture

Study emulator compatibility.

Learn:

```text
instruction traces
breakpoints
step
register dumps
memory watch
quirks
golden screenshots
```

Different historical CHIP-8 variants disagree about some opcode behavior, which is why explicit quirk handling is important.

## Exercises

Build commands:

```text
step
continue
regs
memory
break
disasm
```

## Starter

Debugger REPL skeleton.

## Challenge

Support multiple CHIP-8 quirk profiles and pass the quirk test.

## Solution

Reference solution uses:

```cpp
struct Chip8Quirks;
```

## Coding test

Given a failing ROM plus an execution trace, find and fix the compatibility bug.

## Graduation milestone

You have built your first emulator.

---

# PHASE III — FIRST REAL CPU

# Chapter 7 — Intel 8080 Architecture

## Lecture

Learn:

```text
A B C D E H L
SP
PC
flags
16-bit register pairs
stack
condition codes
instruction timing
```

This is our first **real hardware CPU**.

## Exercises

Implement:

```text
MOV
MVI
LXI
LDA
STA
INR
DCR
ADD
SUB
ANA
ORA
XRA
CMP
```

## Starter

8080 CPU skeleton.

## Challenge

Execute small hand-written 8080 programs.

## Solution

Reference solution separates:

```text
operand read
ALU
flag calculation
write-back
```

## Coding test

Implement ten randomly selected 8080 instructions.

---

# Chapter 8 — 8080 Control Flow, Stack and Interrupts

## Lecture

Study:

```text
CALL
RET
RST
conditional branches
PUSH
POP
interrupt enable
interrupt acknowledge
```

## Exercises

Implement complete control-flow subsystem.

## Starter

Interrupt controller stub.

## Challenge

Pass an 8080 CPU diagnostic ROM.

## Solution

Reference implementation makes instruction timing part of execution results.

## Coding test

Given a trace divergence, identify the first incorrect instruction.

---

# Chapter 9 — Space Invaders Machine

Now CPU emulation becomes **machine emulation**.

## Lecture

Learn:

```text
memory map
I/O ports
shift register hardware
video RAM
interrupt generation
input ports
machine-specific hardware
```

## Exercises

Implement:

```text
ROM mapping
RAM
VRAM
IN
OUT
shift-register peripheral
```

## Starter

Space Invaders board skeleton.

## Challenge

Boot a legally available test/homebrew program using the machine architecture.

## Solution

Reference solution separates:

```text
8080 CPU
      ↓
machine bus
      ↓
device handlers
```

## Coding test

Implement a new fictional 8080 arcade machine from a supplied memory map.

---

# PHASE IV — GAME BOY

Pan Docs remains one of the most comprehensive public Game Boy hardware references and is actively maintained.

# Chapter 10 — LR35902 CPU Architecture

## Lecture

Study:

```text
AF
BC
DE
HL
SP
PC
flags
8-bit instructions
16-bit instructions
CB-prefixed instructions
```

## Exercises

Build instruction metadata:

```cpp
struct Instruction {
    const char* name;
    uint8_t bytes;
    uint8_t cycles;
};
```

Implement basic load/ALU operations.

## Starter

CPU decoder.

## Challenge

Run a CPU-only Game Boy test ROM.

## Solution

Reference implementation generates much of the decode table from metadata.

## Coding test

Implement a randomly selected opcode family.

---

# Chapter 11 — Game Boy CPU Completion

## Lecture

Study:

```text
DAA
rotates
shifts
BIT
SET
RES
HALT
STOP
EI
DI
interrupt behavior
```

## Exercises

Implement remaining instructions.

## Debugging exercise

Fix:

```text
flag errors
HALT behavior
conditional timing
```

## Challenge

Run increasingly strict CPU test suites.

Mooneye provides Game Boy acceptance tests designed for verification against hardware as well as emulator-specific tests.

## Solution

Reference implementation represents cycle differences for taken/not-taken branches explicitly.

## Coding test

Trace-diff an intentionally broken CPU against a known-good trace.

---

# Chapter 12 — Game Boy Bus and Memory Map

## Lecture

Study:

```text
ROM
VRAM
external RAM
WRAM
echo RAM
OAM
I/O
HRAM
interrupt-enable register
```

## Exercises

Implement:

```cpp
uint8_t Bus::read(uint16_t address);
void Bus::write(uint16_t address, uint8_t value);
```

## Starter

Bus routing skeleton.

## Challenge

Implement boot-ROM mapping and unmapping.

## Solution

Reference solution routes hardware ranges through device objects instead of giant global arrays.

## Coding test

Implement an unseen memory-map specification.

---

# Chapter 13 — Game Boy Timers and Interrupts

## Lecture

Learn:

```text
DIV
TIMA
TMA
TAC
IF
IE
VBlank interrupt
LCD interrupt
Timer interrupt
Serial interrupt
Joypad interrupt
```

## Exercises

Implement timer incrementing and overflow.

## Starter

Timer device.

## Challenge

Pass relevant timing tests from Mooneye.

## Solution

Reference implementation models divider state instead of approximating elapsed milliseconds.

## Coding test

Fix a timer that fails three edge cases.

---

# Chapter 14 — Game Boy PPU I

## Lecture

Study:

```text
LCD modes
scanlines
tiles
tile maps
palettes
background
window
```

## Exercises

Render:

```text
one tile
one row
one scanline
one frame
```

## Starter

160×144 framebuffer.

## Challenge

Render a complete background test ROM.

## Solution

Reference implementation initially uses scanline rendering for clarity.

## Coding test

Given VRAM and registers, render one expected scanline exactly.

---

# Chapter 15 — Game Boy PPU II

## Lecture

Study:

```text
sprites
OAM
priority
scrolling
STAT
LY
LYC
PPU modes
VRAM/OAM access restrictions
```

## Exercises

Implement sprites and mode transitions.

## Challenge

Pass multiple PPU test ROMs.

## Solution

Reference PPU is an explicit state machine.

## Coding test

Repair five rasterization/timing defects.

---

# Chapter 16 — Game Boy Cartridges and MBCs

## Lecture

Study:

```text
ROM headers
MBC1
MBC2
MBC3
MBC5
bank switching
battery RAM
RTC
```

## Exercises

Implement cartridge parser.

Then:

```text
MBC1
MBC3
MBC5
```

## Starter

```cpp
class CartridgeController;
```

## Challenge

Boot games requiring bank switching using your own legal dumps/test cartridges.

## Solution

Each mapper is its own strategy.

## Coding test

Implement an unseen simplified mapper specification.

---

# Chapter 17 — Game Boy Audio and Accuracy

## Lecture

Study:

```text
square channels
wave channel
noise
envelope
length counters
frequency sweep
mixing
sample generation
```

## Exercises

Implement each channel separately.

## Starter

Ring-buffer audio backend.

## Challenge

Generate deterministic audio hashes from test programs.

## Solution

Reference APU operates on emulated clock events rather than host audio timing.

## Coding test

Produce a waveform matching golden samples.

## Milestone

A complete Game Boy emulator.

---

# PHASE V — NES

NESdev documents the NES as a 6502-derived CPU plus a separate PPU and cartridge hardware, with CPU and PPU operating concurrently and separate buses—an important jump in emulator complexity.

# Chapter 18 — 6502 CPU

## Lecture

Study:

```text
A X Y
SP
PC
P
addressing modes
status flags
page crossing
```

## Exercises

Implement addressing modes first.

Then implement:

```text
loads
stores
ALU
branches
stack
```

## Starter

6502 execution skeleton.

## Challenge

Match a reference CPU trace.

## Solution

Reference implementation decouples addressing modes from opcode semantics.

## Coding test

Implement ten unseen opcode/address-mode combinations.

---

# Chapter 19 — NES CPU Accuracy

## Lecture

Study:

```text
BRK
IRQ
NMI
RESET
page crossing
dummy accesses
unofficial opcodes
```

NESdev recommends `nestest` as an early CPU validation tool and catalogs additional instruction/timing tests.

## Exercises

Build trace logger compatible with reference logs.

## Challenge

Match `nestest` instruction-by-instruction.

## Solution

Reference logger emits:

```text
PC opcode registers flags cycles
```

## Coding test

Locate the first divergence in a 50,000-instruction trace.

---

# Chapter 20 — NES Bus and Cartridges

## Lecture

Study:

```text
CPU RAM
PPU registers
APU registers
controllers
cartridge
memory mirroring
iNES
```

## Starter

Bus skeleton.

## Exercises

Implement NROM.

## Challenge

Boot an NROM homebrew/test ROM.

## Solution

Reference design introduces:

```cpp
Mapper
```

interface.

## Coding test

Implement a supplied mapper-zero variant.

---

# Chapter 21 — NES PPU I

## Lecture

Study:

```text
pattern tables
nametables
attribute tables
palettes
tiles
scanlines
```

The NES PPU has its own address space and dedicated graphics memory structures.

## Exercises

Render background tiles.

## Starter

PPU bus.

## Challenge

Produce correct framebuffer hashes for simple test ROMs.

## Solution

Reference implementation separates PPU memory from CPU memory.

## Coding test

Render an unseen nametable snapshot.

---

# Chapter 22 — NES PPU II: Scrolling and Sprites

## Lecture

Study:

```text
PPUSCROLL
PPUADDR
sprite OAM
sprite zero hit
sprite overflow
fine X
internal VRAM address registers
```

NES PPU registers are memory mapped through `$2000–$2007`, with mirrored addresses beyond that range.

## Exercises

Implement:

```text
scroll state
sprites
priority
sprite zero
```

## Challenge

Pass selected PPU timing tests.

## Solution

Reference PPU moves toward dot/scanline state.

## Coding test

Diagnose one-pixel and one-scanline timing defects from screenshots/traces.

---

# Chapter 23 — NES Mappers

## Lecture

Study:

```text
bank switching
CHR banks
PRG banks
mirroring
mapper IRQ
```

Implement:

```text
UxROM
CNROM
MMC1
MMC3
```

## Starter

Mapper interface.

## Challenge

Implement MMC3 IRQ behavior.

## Solution

Reference design makes mapper clock hooks explicit.

## Coding test

Implement a fictional mapper from a register specification.

---

# Chapter 24 — NES APU, DMA and Final Synchronization

## Lecture

Study:

```text
pulse
triangle
noise
DMC
OAM DMA
frame counter
CPU/PPU ratio
interrupts
```

## Challenge

Synchronize:

```text
CPU
PPU
APU
mapper
```

fine-grained enough for raster-sensitive behavior. NESdev specifically notes that these components operate concurrently and emulator scheduling must account for that.

## Solution

Reference scheduler advances devices according to master-clock relationships.

## Coding test

Repair a machine that gradually drifts out of synchronization.

## Milestone

Complete NES emulator.

---

# PHASE VI — GAME BOY ADVANCE

GBATEK documents the GBA's ARM7TDMI-based architecture, including both ARM and Thumb instruction modes.

# Chapter 25 — ARM7TDMI ARM Instruction Set

## Lecture

Study:

```text
R0–R15
CPSR
conditions
barrel shifter
data processing
multiply
load/store
branches
```

## Exercises

Implement:

```text
condition evaluation
barrel shifter
ALU
loads/stores
branches
```

## Challenge

Run ARM instruction tests.

## Solution

Reference CPU handles shifter carry separately from ALU carry.

## Coding test

Implement an unseen ARM instruction family.

---

# Chapter 26 — Thumb, Pipeline and Exceptions

## Lecture

Study:

```text
Thumb mode
pipeline
PC semantics
SWI
IRQ
FIQ concepts
banked registers
CPU modes
```

## Exercises

Implement Thumb decoder.

## Challenge

Switch safely between ARM and Thumb.

## Solution

Reference CPU exposes explicit:

```text
fetch
decode
execute
pipeline refill
```

states.

## Coding test

Fix five pipeline/PC bugs.

---

# Chapter 27 — GBA Memory System

## Lecture

Study:

```text
BIOS
EWRAM
IWRAM
I/O
palette RAM
VRAM
OAM
ROM
SRAM
wait states
open bus
```

## Starter

32-bit bus.

## Challenge

Model access width and wait-state behavior.

## Solution

Reference bus returns:

```cpp
struct BusResult {
    uint32_t value;
    unsigned cycles;
};
```

## Coding test

Calculate and implement timing for an unseen sequence of accesses.

---

# Chapter 28 — GBA PPU

## Lecture

Study:

```text
Mode 0
Mode 1
Mode 2
Mode 3
Mode 4
Mode 5
sprites
windows
mosaic
blending
affine backgrounds
```

## Exercises

Implement modes incrementally.

## Challenge

Pass graphics tests from a GBA test suite.

The mGBA project maintains a dedicated public GBA test suite.

## Solution

Reference renderer performs per-scanline composition.

## Coding test

Render a supplied PPU-state snapshot exactly.

---

# Chapter 29 — GBA DMA, Timers and IRQ

## Lecture

Study:

```text
DMA0–DMA3
immediate DMA
HBlank DMA
VBlank DMA
sound FIFO DMA
timers
interrupt controller
```

## Challenge

Create an event scheduler capable of DMA preemption.

## Solution

Reference design schedules hardware events in guest cycles.

## Coding test

Fix a DMA/timer race producing intermittent corruption.

---

# Chapter 30 — GBA Audio, Saves and Accuracy

## Lecture

Study:

```text
Game Boy PSG
Direct Sound
FIFO
SRAM
Flash
EEPROM
RTC concepts
```

## Challenge

Pass:

```text
CPU tests
memory tests
DMA tests
timer tests
graphics tests
```

before enabling commercial compatibility testing.

## Solution

Reference core includes a headless deterministic test runner.

## Coding test

Given a failing suite, isolate one subsystem without running a GUI.

## Milestone

Complete Game Boy Advance emulator.

---

# PHASE VII — SNES

# Chapter 31 — 65C816 CPU

## Lecture

Study:

```text
8/16-bit accumulator
8/16-bit index mode
bank registers
direct page
24-bit addressing
emulation/native modes
```

## Starter

65C816 decoder.

## Challenge

Pass CPU-level tests.

## Solution

Reference CPU keeps operand width explicit in instruction execution.

## Coding test

Implement an unseen combination of addressing mode and operand width.

---

# Chapter 32 — SNES Bus and PPU

## Lecture

Study:

```text
banked memory map
WRAM
VRAM
CGRAM
OAM
background modes
Mode 7
windows
color math
```

## Exercises

Implement:

```text
Mode 0
Mode 1
sprites
windows
```

then Mode 7.

## Challenge

Render a supplied scene state.

## Solution

Reference renderer uses per-layer priority composition.

## Coding test

Fix rendering-order bugs.

---

# Chapter 33 — SNES DMA, HDMA and Audio Subsystem

## Lecture

Study:

```text
DMA
HDMA
scanline effects
SPC700
audio RAM
DSP
CPU/APU communication
multiple clocks
```

## Challenge

Synchronize three major domains:

```text
CPU
PPU
APU
```

## Solution

Reference scheduler uses a common master-time representation.

## Coding test

Fix an HDMA effect that changes one scanline too late.

## Milestone

You are now working on genuinely complicated multi-processor emulation.

---

# PHASE VIII — ADVANCED EMULATOR ENGINEERING

# Chapter 34 — Scheduler Architecture

## Lecture

Compare:

```text
instruction stepping
cycle stepping
event queue
catch-up
master clock
device-local clocks
```

## Exercises

Implement:

```cpp
schedule(timestamp, event);
```

## Starter

Priority-queue scheduler.

## Challenge

Convert one earlier emulator to event scheduling.

## Solution

Reference design uses integer guest clocks—never host wall time—for deterministic hardware state.

## Coding test

Synchronize three fictional devices running at different frequencies.

---

# Chapter 35 — Save States, Rewind and Determinism

## Lecture

Learn:

```text
serialization
versioning
machine state
host state
deterministic replay
rewind buffers
rollback
```

## Exercises

Serialize entire CHIP-8 state.

Then Game Boy state.

## Challenge

Implement:

```text
save
load
rewind 10 seconds
```

## Solution

Reference state contains only emulated-machine state, not frontend resources.

## Coding test

A state loaded 100 times must produce the exact same later framebuffer hash.

---

# Chapter 36 — Emulator Debugger and Developer Tools

## Lecture

Build tools professional emulator developers need:

```text
disassembler
breakpoints
watchpoints
memory viewer
register viewer
VRAM viewer
tile viewer
sprite viewer
trace logger
instruction history
```

## Challenge

Create an integrated debugger for one previous system.

## Solution

Reference debugger uses generic CPU-debug interfaces.

## Coding test

Diagnose an unseen ROM failure using only debugger tools.

---

# Chapter 37 — Performance and Dynamic Recompilation

## Lecture

Study:

```text
dispatch overhead
decode caching
computed dispatch
basic blocks
IR
code cache
dynamic recompilation
invalidation
self-modifying code
```

## Starter

Build a toy 8-register RISC interpreter.

## Challenge

Translate basic blocks into a tiny host-independent IR.

## Solution

Reference pipeline:

```text
guest code
↓
decode
↓
IR
↓
optimization
↓
execute
```

A native JIT is optional here.

## Coding test

Optimize an interpreter by at least a defined benchmark threshold without changing output hashes.

---

# PHASE IX — PLAYSTATION 1

PSX-SPX documents essentially the full PS1 machine surface, including memory and I/O maps, GPU, GTE, MDEC, SPU, interrupts, DMA, timers, CD-ROM, controllers, memory cards and CPU behavior.

# Chapter 38 — MIPS R3000A CPU

## Lecture

Study:

```text
32 GPRs
HI
LO
PC
COP0
load/store
branches
branch delay slots
exceptions
unaligned operations
```

## Exercises

Implement:

```text
ALU
shifts
branches
loads/stores
multiply/divide
```

## Starter

MIPS interpreter skeleton.

## Challenge

Run CPU test executables.

## Solution

Reference execution explicitly tracks:

```text
current_pc
next_pc
delay_slot
```

## Coding test

Fix branch-delay-slot bugs in an intentionally broken CPU.

---

# Chapter 39 — PS1 Exceptions, Coprocessor 0 and Memory

## Lecture

Study:

```text
exceptions
COP0
status
cause
EPC
KSEG0
KSEG1
RAM
scratchpad
BIOS
memory control
```

## Challenge

Boot far enough into a BIOS sequence to execute exception-related behavior.

## Solution

Reference exception entry updates the architectural state rather than directly jumping.

## Coding test

Implement exception handling for an unseen sequence.

---

# Chapter 40 — PS1 Interrupts and Timers

## Lecture

Study:

```text
I_STAT
I_MASK
Timer0
Timer1
Timer2
sources
modes
targets
```

## Starter

Interrupt-controller device.

## Challenge

Run timer/interrupt hardware tests.

## Solution

Reference design treats interrupts as level/state conditions rather than ordinary function calls.

## Coding test

Fix an interrupt that is acknowledged incorrectly.

---

# Chapter 41 — PS1 GPU I: Commands and VRAM

## Lecture

Study:

```text
GP0
GP1
VRAM
command FIFO
rectangles
lines
polygons
textures
CLUT
```

## Exercises

Implement:

```text
VRAM clear
rectangle
flat triangle
Gouraud triangle
```

## Starter

Software rasterizer.

## Challenge

Render test primitives matching reference VRAM output.

## Solution

Reference GPU uses integer/fixed-point behavior where required.

## Coding test

Rasterize an unseen triangle state.

---

# Chapter 42 — PS1 GPU II: Texturing and Rasterization Accuracy

## Lecture

Study:

```text
texture pages
CLUT
4-bit textures
8-bit textures
15-bit textures
transparency
dithering
drawing area
drawing offset
mask bits
```

## Challenge

Support textured polygons.

## Solution

Reference implementation separates:

```text
primitive setup
rasterizer
texture fetch
pixel blend
```

## Coding test

Match a reference framebuffer hash.

---

# Chapter 43 — PS1 DMA

## Lecture

Study:

```text
DMA channels
GPU DMA
OTC
CD-ROM DMA
SPU DMA
MDEC DMA
linked-list DMA
chopping
```

## Exercises

Implement DMA controller.

## Challenge

Feed GPU command lists through linked-list DMA.

## Solution

Reference controller exposes each device through a DMA endpoint.

## Coding test

Repair a linked-list chain containing termination and boundary edge cases.

---

# Chapter 44 — Geometry Transformation Engine

## Lecture

Study:

```text
COP2
vectors
matrices
fixed point
perspective transformation
lighting
clamping
GTE flags
pipeline timing
```

## Exercises

Implement foundational GTE operations.

## Challenge

Run GTE conformance tests.

The public `ps1-tests` project contains CPU, DMA, GPU, GTE, MDEC, SPU, timer, CD-ROM and controller-oriented tests useful for emulator verification.

## Solution

Reference GTE separates mathematical intermediate widths from architectural saturation.

## Coding test

Implement one unseen GTE opcode from specification.

---

# Chapter 45 — PS1 CD-ROM

## Lecture

Study:

```text
CD controller
commands
responses
interrupts
sectors
BIN/CUE
Mode 2
XA
seek timing concepts
```

## Starter

Disc-image parser.

## Challenge

Implement:

```text
GetStat
Setloc
ReadN
Pause
Seek
```

state transitions.

## Solution

Reference CD controller is asynchronous and event-driven.

## Coding test

Given a command sequence, produce the correct response/interrupt sequence.

---

# Chapter 46 — MDEC Video Decoder

## Lecture

Study:

```text
macroblocks
RLE
quantization
IDCT
YUV
RGB conversion
DMA
```

## Starter

Decode one synthetic macroblock.

## Challenge

Decode a small test frame.

## Solution

Reference implementation separates:

```text
bitstream
RLE
IDCT
color conversion
```

## Coding test

Decode a supplied compressed block to an exact pixel hash.

---

# Chapter 47 — PS1 SPU

## Lecture

Study:

```text
24 voices
ADPCM
pitch
volume
ADSR
noise
reverb
SPU RAM
CD audio
IRQ
```

## Exercises

### TODO1

Decode PS1 ADPCM.

### TODO2

Voice playback.

### TODO3

Pitch stepping.

### TODO4

ADSR.

### TODO5

Mixing.

## Challenge

Pass selected SPU tests.

## Solution

Reference SPU processes audio in guest sample time.

## Coding test

Produce expected PCM output from an unseen ADPCM stream.

---

# Chapter 48 — Controllers, Memory Cards and Serial I/O

## Lecture

Study:

```text
JOY/SIO
controller commands
digital pad
memory-card protocol
card blocks
checksums
```

## Starter

Digital controller.

## Challenge

Create a working virtual memory card.

## Solution

Reference design implements peripherals as serial protocol devices.

## Coding test

Respond correctly to an unseen controller/card transaction log.

---

# Chapter 49 — Complete PS1 System Scheduling

This is one of the most important chapters.

## Lecture

Your emulator now contains:

```text
CPU
GPU
DMA
GTE
Timers
Interrupts
CDROM
MDEC
SPU
Controller
```

All run concurrently.

Study:

```text
device clocks
event scheduling
DMA stalls
GPU busy state
CD latency
SPU timing
interrupt ordering
```

## Starter

Replace ad-hoc stepping with the common scheduler from Chapter 34.

## Challenge

Boot an increasingly complex PS1 homebrew/test environment deterministically.

## Solution

Reference architecture:

```text
CPU executes
     ↓
scheduler time advances
     ↓
due events dispatched
     ↓
devices update
     ↓
interrupt state changes
     ↓
CPU continues
```

## Coding test

Repair a timing-sensitive failure caused by two events occurring in the wrong order.

---

# Chapter 50 — PS1 Accuracy, Trace Testing and Compatibility

## Lecture

Learn the difference between:

```text
"boots a game"

and

"correct emulator"
```

Build:

```text
CPU trace testing
GPU VRAM hashes
SPU sample hashes
DMA tests
GTE tests
timer tests
CD-ROM state tests
```

The PS1 ecosystem has public emulator-development test suites covering many of these subsystems.

## Exercises

Integrate test ROM execution into:

```bash
make accuracy
```

## Challenge

Make all mandatory PS1 course tests pass.

## Solution

Reference test runner can operate without SDL or a GUI.

## Coding test

You receive ten regressions.

Find and fix all of them.

---

# Chapter 51 — PS1 Full Emulator Capstone

No starter implementation.

You have the components from previous chapters, but you must build the final integration yourself.

Architecture:

```text
                    ┌────────────┐
                    │   R3000A   │
                    └──────┬─────┘
                           │
                      System Bus
                           │
        ┌──────────┬───────┼────────┬───────────┐
        ↓          ↓       ↓        ↓           ↓
       RAM        GPU     DMA      SPU        CD-ROM
                   │       │
                   │       ├──── GPU
                   │       ├──── SPU
                   │       ├──── CD
                   │       └──── MDEC
                   │
                  GTE
```

## Requirements

Your final emulator must contain:

```text
R3000A CPU
COP0
GTE
RAM
BIOS mapping
interrupt controller
timers
DMA
GPU
MDEC
CD-ROM
SPU
controller
memory card
scheduler
debugger
save states
headless tests
```

---

# Final Challenge

Given only:

```text
PS1 specification
your previous reusable libraries
public test programs
```

make the machine reach defined test milestones.

No step-by-step implementation guide.

---

# Final Coding Test

You receive:

```text
MiniConsole-32
```

A fictional machine inspired by everything you have learned.

Specification:

```text
32-bit RISC CPU

16 registers

branch delay slots

2 MB RAM

memory-mapped GPU

DMA controller

timer

interrupt controller

tile/triangle graphics

4-channel audio

gamepad

ROM cartridge
```

Nothing is implemented.

Your task:

```text
read specification
      ↓
design architecture
      ↓
implement CPU
      ↓
implement bus
      ↓
implement GPU
      ↓
implement DMA
      ↓
implement timers
      ↓
implement interrupts
      ↓
implement audio
      ↓
integrate
      ↓
pass hidden ROMs
```

The hidden tests ensure that you have learned **emulator engineering**, rather than memorized PS1 code.

---

# 52. Testing Philosophy

Starting from CHIP-8, every emulator should be testable without opening a window.

Example:

```bash
./emu \
    --rom tests/cpu.bin \
    --headless \
    --cycles 100000 \
    --trace result.log
```

Then:

```bash
python tools/compare_trace.py \
    expected.log \
    result.log
```

---

# 53. Test Pyramid

Every chapter eventually uses four layers.

```text
           ┌─────────────┐
           │ Game tests  │
           └──────┬──────┘
             System tests
           ┌──────┴──────┐
           │ Hardware ROM│
           └──────┬──────┘
           Component tests
           ┌──────┴──────┐
           │ CPU/PPU/DMA │
           └──────┬──────┘
              Unit tests
           ┌──────┴──────┐
           │ ALU / decode│
           └─────────────┘
```

---

# 54. Trace-First Debugging

When a game fails, don't immediately stare at the screen.

Use:

```text
known-good emulator
      │
      ├── trace A
      │
your emulator
      │
      └── trace B
```

Then:

```text
compare

PC
instruction
registers
flags
memory access
cycles
```

Find:

```text
first divergence
```

not:

```text
last visible symptom
```

This becomes mandatory from the 8080 phase onward.

---

# 55. Every CPU Must Have a Disassembler

Mandatory interface:

```cpp
std::string disassemble(uint32_t pc);
```

Example:

```text
80010000: 3C011F80  LUI   AT, 0x1F80
80010004: 34211000  ORI   AT, AT, 0x1000
```

You aren't allowed to wait until the debugger chapter to create one.

---

# 56. Every Emulator Must Support Stepping

Mandatory:

```cpp
StepResult step();
```

Returning something like:

```cpp
struct StepResult {
    uint64_t cycles;
    uint32_t pc;
};
```

This is fundamental for:

```text
testing
debugging
tracing
scheduling
```

---

# 57. Every Device Must Be Independently Testable

Bad:

```cpp
class Emulator {
    // 20,000 lines
};
```

Preferred:

```text
Cpu
Bus
Ppu
Apu
Timer
Dma
Gpu
Spu
Cdrom
Cartridge
Scheduler
```

Tests should be able to instantiate:

```cpp
Gpu gpu;
```

without creating an entire PlayStation.

---

# 58. Every Hardware Register Gets a Specification Test

Example:

```cpp
TEST(PpuCtrl, SpritePatternTableBit)
```

or:

```cpp
TEST(DmaChannel, StartsWhenEnableAndTriggerSet)
```

This is especially important for:

```text
NES PPU
GBA DMA
SNES HDMA
PS1 DMA
PS1 GPU
PS1 SPU
```

---

# 59. Mandatory Debugging Exercises

Every major phase contains broken implementations.

Example defects:

```text
wrong carry

incorrect endian decoding

PC advanced twice

wrong branch-delay behavior

timer off by one

DMA completes early

PPU changes mode one cycle late

sprite priority inverted

audio envelope off by one

mapper bank mask wrong

CD interrupt delivered too early
```

Your task is not just:

> Fix it.

You must also produce:

```text
bug
root cause
first observable divergence
fix
regression test
```

---

# 60. Progress Tracking

```markdown
# Emulator School

Current Chapter: 1
Highest Unlocked Chapter: 1

| Chapter | Exercises | Starter | Debug | Challenge | Code Test | Status |
|---|---|---|---|---|---|---|
| 1 | - | - | - | - | - | ACTIVE |
| 2 | - | - | - | - | - | LOCKED |
| 3 | - | - | - | - | - | LOCKED |
...
| 51 | - | - | - | - | - | LOCKED |
```

A successful chapter becomes:

```text
Chapter 14

Exercises       ✓
Starter         ✓
Debugging       ✓
Challenge       ✓
Code Test       ✓

STATUS: PASSED
```

Only then:

```text
Chapter 15

STATUS: ACTIVE
```

---

# 61. Difficulty Curve

## CHIP-8

```text
Lecture    ██████████
Starter    ██████████
Hints      ██████████
Challenge  █████
```

## Game Boy

```text
Lecture    ████████
Starter    ███████
Hints      ██████
Challenge  ███████
```

## NES/GBA

```text
Lecture    ██████
Starter    █████
Hints      ████
Challenge  ████████
```

## SNES

```text
Lecture    █████
Starter    ███
Hints      ███
Challenge  █████████
```

## PS1

```text
Lecture    ████
Starter    ██
Hints      ██
Challenge  ██████████
```

## Final

```text
Lecture    █
Starter

Hints

Specification ██████████
Your code      ██████████
```

---

# 62. Primary Reference Progression

## CHIP-8

Use Tobias Langhoff's CHIP-8 architecture guide and the Timendus CHIP-8 test suite.

## Game Boy

Primary hardware reference:

```text
Pan Docs
```

with Mooneye as a major testing resource.

## NES

Primary reference:

```text
NESdev Wiki
```

including its CPU, PPU, mapper, timing, and emulator-test material.

## GBA

Primary reference:

```text
GBATEK
```

especially the ARM7TDMI, memory map, I/O, LCD, DMA, timers and cartridge sections.

Testing can include the mGBA test suite.

## PS1

Primary machine reference:

```text
PSX-SPX
```

which indexes essentially every major PS1 hardware subsystem.

Testing:

```text
ps1-tests
```

covering CPU and several major hardware subsystems.

---

# 63. What You Should Be Able to Do at Each Milestone

## After CHIP-8

```text
fetch
decode
execute
timers
input
framebuffer
debugging
```

## After Space Invaders

```text
real ISA
hardware bus
interrupts
I/O devices
```

## After Game Boy

```text
full console architecture
scanline graphics
banking
timers
audio
```

## After NES

```text
multiple hardware clocks
PPU timing
DMA
cartridge hardware
mapper IRQ
```

## After GBA

```text
32-bit RISC
CPU pipeline
ARM/Thumb
complex DMA
advanced 2D graphics
```

## After SNES

```text
multi-processor synchronization
HDMA
advanced raster effects
separate sound CPU/DSP
```

## After PS1

```text
32-bit RISC system
branch delay slots
coprocessors
DMA fabric
3D GPU
geometry accelerator
CD-ROM
video decoder
24-voice audio processor
complex event scheduling
```

---

# 64. Graduation Standard

You graduate when I can give you something resembling:

```text
MysteryConsole Technical Manual
```

containing:

```text
CPU instruction reference
memory map
register descriptions
graphics specification
audio specification
interrupt table
DMA documentation
cartridge/disc format
```

and you can independently turn that into:

```text
specification
    ↓
architecture
    ↓
CPU
    ↓
bus
    ↓
hardware devices
    ↓
scheduler
    ↓
test harness
    ↓
debugger
    ↓
working emulator
```

without needing:

> “How to write a MysteryConsole emulator” tutorial.

That is the point at which you have stopped learning individual consoles and started understanding **emulation itself**.

---

# 65. How We Will Actually Take the Course

When you say:

```text
Start Chapter 1
```

I should provide only:

```text
Chapter 1 lecture
        ↓
Lab objectives
        ↓
environment
        ↓
starter repository
        ↓
Exercise 1.1
```

You submit the code.

Then:

```text
CODE REVIEW
   │
   ├── PASS → Exercise 1.2
   │
   └── REVISE
          ↓
        hints
          ↓
        resubmit
```

Eventually:

```text
all exercises
      ↓
challenge
      ↓
coding test
      ↓
PASS
      ↓
Chapter 2 unlocked
```

The next chapter does **not** unlock simply because you want to skip ahead.

The purpose is to make this behave like a real systems laboratory course modeled after Linux Kernel Labs—not a playlist of emulator tutorials.