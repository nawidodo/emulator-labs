# Emulator Foundations — Strict C17
## CHIP-8 → 6502 → NES → PS1 Entry Gate

This is the canonical prerequisite course for the dedicated PS1 emulator curriculum.

Its purpose is **not** to make you spend years finishing every older console. Its purpose is to give you the mental model needed to understand a computer from the ground up:

```text
bytes
  ↓
bits / integer representation
  ↓
registers
  ↓
fetch → decode → execute
  ↓
real CPU behavior
  ↓
bus / memory map
  ↓
MMIO
  ↓
interrupts / timers
  ↓
graphics device
  ↓
audio device
  ↓
DMA
  ↓
controller protocol
  ↓
multiple devices sharing emulated time
  ↓
complete computer
```

## Language and portability policy

```text
core implementation   → strict ISO C17
public boundary       → stable C ABI
host/UI               → replaceable
platform APIs in core → forbidden
```

The required core should be usable without SDL or any UI framework. A frontend may later be SDL, C#/WinUI, Swift/UIKit, Swift/AppKit, or something else.

A core source file must never need headers such as:

```c
#include <windows.h>
#include <UIKit/UIKit.h>
#include <Metal/Metal.h>
#include <SDL.h>
```

Those belong outside the machine core.

## Lab methodology

Every chapter uses the same gated workflow:

1. required reading / lecture,
2. starter skeleton,
3. numbered TODOs,
4. deterministic public tests,
5. debugging exercise,
6. challenge,
7. hidden/unseen test,
8. **PASS / REVISE**.

If a full solution is revealed early, mark the exercise `SOLVED_WITH_REFERENCE` and require a fresh unseen equivalent test.

## Repository target

```text
emulator-foundations/
├── common/
├── chip8/
├── cpu6502/
├── nes/
│   ├── core/
│   │   ├── cpu/
│   │   ├── bus/
│   │   ├── cart/
│   │   ├── ppu/
│   │   ├── apu/
│   │   ├── input/
│   │   └── state/
│   └── include/
│       └── nes.h
├── frontends/
│   └── optional/
└── tests/
```


# Phase 0 — Lab Infrastructure and C17 Foundations

## Chapter 1 — Course Contract and Repository Layout

**Goal:** Set up the gated Linux-Kernel-Labs-style workflow: templates, skeletons, TODO checkpoints, public tests, hidden tests, and strict sequential unlocking.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 2 — C17 Integer Model for Emulation

**Goal:** Master exact-width integers, shifts, masks, sign extension, wrapping arithmetic, promotions, and endian-safe byte assembly.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 3 — Pointers, Arrays, Structs, and Ownership

**Goal:** Implement and test pointers, arrays, structs, and ownership as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 4 — Build System, Assertions, and Sanitizers

**Goal:** Implement and test build system, assertions, and sanitizers as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 5 — Deterministic Test Harness

**Goal:** Implement and test deterministic test harness as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 6 — Hex Dumps, Trace Files, and First-Divergence Debugging

**Goal:** Implement and test hex dumps, trace files, and first-divergence debugging as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 7 — Portable Core vs Host Frontend

**Goal:** Establish the rule that the emulator core knows nothing about SDL, UIKit, AppKit, Win32, Metal, Direct3D, speakers, keyboards, or filesystems.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 8 — Stable C ABI Primer

**Goal:** Learn opaque handles, fixed-width public types, versioned structs, and why a C ABI is a strong cross-language boundary.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.


# Phase 1 — CHIP-8: Learn the Shape of a Machine

## Chapter 9 — CHIP-8 Machine State

**Goal:** Model the complete machine explicitly: memory, V registers, I, PC, stack, timers, keypad, and framebuffer.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 10 — ROM Loading and Address Space

**Goal:** Implement and test rom loading and address space as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 11 — Fetch

**Goal:** Implement instruction fetch from byte-addressed memory and make PC changes explicit.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 12 — Decode

**Goal:** Decode opcodes with masks and shifts and separate decoding from execution.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 13 — Control Flow

**Goal:** Implement and test control flow as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 14 — ALU and VF Semantics

**Goal:** Implement and test alu and vf semantics as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 15 — Index Register and Memory Transfers

**Goal:** Implement and test index register and memory transfers as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 16 — Randomness as an Injected Service

**Goal:** Implement and test randomness as an injected service as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 17 — Framebuffer and XOR Sprite Drawing

**Goal:** Implement the first graphics device and learn collision behavior through XOR rasterization.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 18 — Timers and 60 Hz Timebase

**Goal:** Separate emulated device time from instruction count and host wall-clock time.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 19 — Keypad Input

**Goal:** Implement and test keypad input as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 20 — Compatibility Quirks

**Goal:** Implement and test compatibility quirks as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 21 — Instruction Trace and Debugger

**Goal:** Implement and test instruction trace and debugger as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 22 — CHIP-8 Validation Gate

**Goal:** Pass deterministic CHIP-8 tests and explain fetch/decode/execute, timing, input, and video end to end.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.


# Phase 2 — 6502: A Real CPU

## Chapter 23 — 6502 Architectural State

**Goal:** Model A, X, Y, SP, PC, flags, and the CPU-visible bus.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 24 — 6502 Bus Interface

**Goal:** Route every CPU memory access through a bus instead of directly indexing RAM.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 25 — Opcode Table and Instruction Metadata

**Goal:** Implement and test opcode table and instruction metadata as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 26 — Immediate and Implied Addressing

**Goal:** Implement and test immediate and implied addressing as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 27 — Zero Page Addressing

**Goal:** Implement and test zero page addressing as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 28 — Absolute Addressing

**Goal:** Implement and test absolute addressing as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 29 — Indirect Addressing and the JMP Bug

**Goal:** Learn the rule 'emulate hardware behavior, even when it looks wrong' by reproducing the 6502 indirect-JMP wrap bug.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 30 — Indexed-Indirect and Indirect-Indexed

**Goal:** Implement and test indexed-indirect and indirect-indexed as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 31 — Load and Store Instructions

**Goal:** Implement and test load and store instructions as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 32 — Transfer and Stack Instructions

**Goal:** Implement and test transfer and stack instructions as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 33 — Arithmetic: ADC

**Goal:** Implement and test arithmetic: adc as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 34 — Arithmetic: SBC and Compare

**Goal:** Implement and test arithmetic: sbc and compare as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 35 — Logic and Bit Operations

**Goal:** Implement and test logic and bit operations as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 36 — Branches and Relative Offsets

**Goal:** Implement and test branches and relative offsets as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 37 — Jumps, Subroutines, and Returns

**Goal:** Implement and test jumps, subroutines, and returns as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 38 — Interrupt Model: RESET, IRQ, NMI, BRK

**Goal:** Implement vectoring, stack state, status handling, and interrupt return.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 39 — Cycle Accounting

**Goal:** Make cycles a first-class result of CPU execution.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 40 — Unofficial Opcodes: Policy and Diagnostics

**Goal:** Implement and test unofficial opcodes: policy and diagnostics as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 41 — 6502 Trace Harness

**Goal:** Implement and test 6502 trace harness as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 42 — 6502 CPU Validation Gate

**Goal:** Pass instruction, addressing, interrupt, and cycle tests before integrating the CPU into the NES.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.


# Phase 3 — NES System Architecture, Bus, and Cartridge

## Chapter 43 — NES Hardware Overview

**Goal:** Understand the NES as interacting CPU, PPU, APU, RAM, cartridge, controllers, DMA, clocks, and interrupt lines.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 44 — CPU Memory Map

**Goal:** Implement RAM mirroring, cartridge space, and memory-mapped device registers.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 45 — Cartridge File Format: iNES 1.0

**Goal:** Implement and test cartridge file format: ines 1.0 as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 46 — PRG ROM and CHR Storage

**Goal:** Implement and test prg rom and chr storage as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 47 — Mapper Interface

**Goal:** Implement and test mapper interface as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 48 — Mapper 0 / NROM

**Goal:** Build the simplest complete cartridge path needed for the PS1 prerequisite gate.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 49 — PPU Memory Map

**Goal:** Implement and test ppu memory map as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 50 — PPU Register Interface

**Goal:** Model CPU-visible PPU registers including their side effects, latches, and buffered reads.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 51 — Open Bus and Unimplemented Access Diagnostics

**Goal:** Implement and test open bus and unimplemented access diagnostics as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.


# Phase 4 — NES PPU and Video

## Chapter 52 — PPU Timing Model

**Goal:** Advance the PPU by scanline/dot time and model visible lines, VBlank, and the pre-render line.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 53 — Background Tile Fetch Pipeline

**Goal:** Implement and test background tile fetch pipeline as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 54 — Scrolling Registers v/t/x/w

**Goal:** Learn the NES PPU's internal scroll/address state instead of treating scrolling as a simple X/Y variable.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 55 — Background Pixel Selection

**Goal:** Implement and test background pixel selection as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 56 — OAM and Sprite Data

**Goal:** Implement and test oam and sprite data as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 57 — Sprite Evaluation

**Goal:** Implement and test sprite evaluation as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 58 — Sprite Pattern Fetch and Flipping

**Goal:** Implement and test sprite pattern fetch and flipping as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 59 — Sprite/Background Composition

**Goal:** Implement and test sprite/background composition as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 60 — Sprite Zero Hit

**Goal:** Implement a timing-sensitive signal used by real games.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 61 — Palette and NES Color Output

**Goal:** Implement and test palette and nes color output as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 62 — Frame Boundary and Video Buffer API

**Goal:** Expose complete frames through a host-neutral C structure without giving the PPU a windowing dependency.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 63 — PPU Validation Gate

**Goal:** Implement and test ppu validation gate as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.


# Phase 5 — Input, DMA, Audio, and Whole-Machine Timing

## Chapter 64 — NES Controller Electrical/Serial Model

**Goal:** Model strobe/latch/shift behavior so host buttons become real emulated controller reads.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 65 — Host Input Boundary

**Goal:** Implement and test host input boundary as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 66 — OAM DMA

**Goal:** Implement CPU-memory-to-OAM DMA including CPU stall timing.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 67 — APU Architecture Overview

**Goal:** Understand pulse, triangle, noise, DMC, envelopes, counters, frame sequencing, and mixing before implementing channels.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 68 — Pulse Channels

**Goal:** Implement and test pulse channels as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 69 — Triangle Channel

**Goal:** Implement and test triangle channel as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 70 — Noise Channel

**Goal:** Implement and test noise channel as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 71 — Frame Counter

**Goal:** Implement and test frame counter as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 72 — Basic Mixer and PCM Output

**Goal:** Turn emulated APU state into deterministic PCM samples for a host to consume.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 73 — DMC Concepts and Deferred Accuracy

**Goal:** Understand DMC and its DMA interactions while explicitly keeping perfect DMC accuracy out of the PS1 prerequisite gate.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 74 — Master Scheduler

**Goal:** Advance CPU, PPU, APU, DMA, and interrupts under one deterministic emulated timeline.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 75 — NMI/IRQ Integration

**Goal:** Implement and test nmi/irq integration as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 76 — Audio/Video Synchronization

**Goal:** Keep emulated timing separate from host presentation and audio playback.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.


# Phase 6 — Portable Engine Boundary and Debugging

## Chapter 77 — Opaque NES Handle and Public C API

**Goal:** Hide internal state behind an opaque `nes_t *` suitable for Swift, C#, Rust, Python, or C hosts.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 78 — Versioned Configuration Structs

**Goal:** Design ABI-friendly configuration structures with `struct_size`, version fields, and fixed-width members.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 79 — ROM Loading Boundary

**Goal:** Have the host provide ROM bytes so the core does not depend on filesystem APIs.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 80 — Video Frame ABI

**Goal:** Expose pixels, dimensions, pitch, and format through a stable host-neutral boundary.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 81 — Audio FIFO ABI

**Goal:** Expose deterministic PCM through a producer/consumer FIFO API.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 82 — Input ABI

**Goal:** Expose controller state through fixed-width C types that foreign languages can reproduce safely.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 83 — Save-State Design

**Goal:** Serialize machine state deliberately; do not make raw in-memory struct layout the save-state format.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 84 — Debugger Core

**Goal:** Build stepping, traces, register inspection, bus diagnostics, and breakpoints without requiring a GUI.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 85 — Golden Frame and Audio Regression

**Goal:** Implement and test golden frame and audio regression as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.


# Phase 7 — Playable NES Gate (Required Before PS1)

## Chapter 86 — Boot a Tiny NROM Homebrew

**Goal:** Move from subsystem tests into controlled whole-machine software.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 87 — Boot a Simple NROM Game

**Goal:** Reach stable, interactive gameplay with CPU, PPU, cartridge, NMI, controllers, and basic audio working together.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 88 — Playable Input Gate

**Goal:** Implement and test playable input gate as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 89 — Video Gate

**Goal:** Implement and test video gate as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 90 — Basic Audio Gate

**Goal:** Implement and test basic audio gate as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 91 — Determinism Gate

**Goal:** Prove the same ROM and input stream produce identical state, frame, and audio hashes.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 92 — Portable-Core Gate

**Goal:** Prove no platform headers live in `core/` and that the public C API is sufficient to host the emulator.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 93 — NES Final Explain Gate

**Goal:** Explain one controller press all the way through CPU game logic to final pixels, plus one APU sample path. Passing this unlocks PS1.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.


# Phase 8 — Optional NES Extensions (Not PS1 Prerequisites)

## Chapter 94 — Mapper 2 / UxROM

**Goal:** Implement and test mapper 2 / uxrom as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 95 — Mapper 3 / CNROM

**Goal:** Implement and test mapper 3 / cnrom as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 96 — Mapper 1 / MMC1

**Goal:** Implement and test mapper 1 / mmc1 as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 97 — Mapper 4 / MMC3 Concepts

**Goal:** Implement and test mapper 4 / mmc3 concepts as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 98 — More Accurate DMC

**Goal:** Implement and test more accurate dmc as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 99 — Cycle-Accurate PPU Edge Cases

**Goal:** Implement and test cycle-accurate ppu edge cases as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 100 — Rewind and Run-Ahead

**Goal:** Implement and test rewind and run-ahead as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 101 — Movie Input Recording

**Goal:** Implement and test movie input recording as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 102 — Frontend Branches

**Goal:** Implement and test frontend branches as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 103 — Performance Profiling

**Goal:** Implement and test performance profiling as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.

## Chapter 104 — Optional GBA/SNES Study Branch

**Goal:** Implement and test optional gba/snes study branch as an explicit part of the machine model, keeping behavior deterministic and hardware-driven.

**Required work**
- implement only the currently unlocked TODOs;
- keep the core deterministic;
- keep CPU/device state explicit in C structs;
- route hardware-visible memory through the appropriate bus;
- add a focused unit/integration test;
- log unknown behavior instead of silently inventing values;
- debug by first divergence.

**Gate:** the chapter's public tests and one unseen equivalent test must pass before continuing.


# Required PS1 Entry Gate

The required path is **Chapters 1–93**.

Passing the gate means you have a playable simple NROM NES with:

- validated 2A03/6502 CPU behavior,
- CPU and PPU buses,
- NROM cartridge support,
- working background/sprite PPU path within course scope,
- controller serial protocol,
- OAM DMA,
- basic APU audio,
- deterministic CPU/PPU/APU/DMA scheduling,
- headless tests,
- debugger/tracing,
- save-state foundations,
- host-neutral input/video/audio interfaces,
- opaque C handle / stable C ABI.

You must be able to explain:

```text
host button
   ↓
C API input state
   ↓
controller strobe/latch/shift
   ↓
CPU MMIO read
   ↓
6502 game logic
   ↓
PPU state/memory
   ↓
pixel composition
   ↓
host-neutral framebuffer
   ↓
frontend
```

and:

```text
CPU writes APU register
   ↓
APU timers / envelopes / counters
   ↓
channel sample
   ↓
mixer
   ↓
PCM FIFO
   ↓
host audio backend
```

and:

```text
CPU consumes cycles
   ↓
master emulated time
   ↓
PPU / APU / DMA / IRQ advance deterministically
```

At that point:

```text
NES FOUNDATION GATE
        ↓ PASS
Dedicated PS1 C17 Curriculum
        ↓
MIPS R3000A
bus / MMIO
DMA
GPU / VRAM
software rasterizer
GTE
CD-ROM / MDEC
SPU
SIO / memory cards
BIOS / disc boot
JIT
```

# What Is Explicitly NOT Required Before PS1

Do not block PS1 progress on:

- every NES mapper,
- perfect compatibility with every NES game,
- perfect DMC timing,
- every obscure PPU race,
- rewind,
- run-ahead,
- netplay,
- shaders,
- polished GUI,
- GBA,
- SNES.

Those are optional branches after Chapter 93.

# Concept Mapping: NES → PS1

| NES | PS1 |
|---|---|
| 2A03 / 6502 | MIPS R3000A |
| CPU bus | PS1 bus / MMIO |
| PPU | GPU |
| APU | SPU |
| OAM DMA | multi-channel DMA controller |
| NMI/IRQ | interrupt controller + device IRQs |
| cartridge | CD-ROM subsystem |
| controller port | SIO |
| sprite/background rendering | GP0 / VRAM / rasterization |
| CPU/PPU/APU scheduling | CPU/GPU/SPU/DMA/CD/timer scheduling |

# Recommended Reference Stack

## CHIP-8
- Timendus CHIP-8 test suite
- a documented CHIP-8 specification/quirk reference

## 6502
- trusted opcode/addressing/cycle reference
- functional CPU test suites and trace fixtures

## NES
- NESdev Wiki as the main hardware reference
- public CPU/PPU/APU test ROMs
- Yi Zhang's NES emulator series as an emulator-architecture companion
- Bryan Lee's *Writing an NES Emulator* as a companion

## PS1 handoff
- PSX-SPX as hardware/register reference
- Simias' PSX Guide as a narrative implementation companion
- ps1-tests for behavioral validation

# Final Principle

The goal is not:

> “the game seems to run.”

The goal is:

> **I can trace an instruction from bytes, through CPU decode and execution, through bus/MMIO interaction with another hardware block, through emulated time, and explain exactly why the resulting pixel, sound, interrupt, or controller value exists.**
