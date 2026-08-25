# Roadmap & Milestones

Nine phases, 51 chapters, one graduation exam.

| Phase | Chapters | New concepts | Milestone: you can... |
|---|---|---|---|
| I Foundations | 1-2 | lab workflow, fetch/decode/execute | build a fictional CPU from spec |
| II CHIP-8 | 3-6 | ISA fields, framebuffer, timers, quirks, debugger | finish your first complete emulator |
| III First real CPU | 7-9 | real ISA (8080), interrupts, I/O ports, machine bus | emulate a full arcade board |
| IV Game Boy | 10-17 | bus/memory map, scanline PPU, MBC banking, PSG audio | complete console emulation |
| V NES | 18-24 | concurrent CPU/PPU clocks, mappers, mapper IRQ, OAM DMA | multi-clock synchronization |
| VI GBA | 25-30 | ARM7TDMI + Thumb, pipelines, DMA fabric, wait states | 32-bit RISC system emulation |
| VII SNES | 31-33 | 65C816 widths/banks, Mode 7, HDMA, SPC700 domain | multi-processor machines |
| VIII Engineering | 34-37 | event schedulers, save states/rewind, tooling, IR/dynarec | emulator *engineering* beyond one machine |
| IX PS1 | 38-51 | R3000A delay slots, COP0/GTE, GPU raster, DMA fabric, CD-ROM, MDEC, SPU, system scheduling | PS1-class emulator + capstone |

## Graduation standard

Given only a technical manual for an unfamiliar machine ("MysteryConsole"),
produce specification → architecture → CPU → bus → devices → scheduler →
tests → debugger → working emulator without a tutorial. The Final Coding
Test (`docs/final-challenge.md`, MiniConsole-32) is exactly that exam.

## Primary references

| System | Reference | Testing |
|---|---|---|
| CHIP-8 | Tobias Langhoff's guide; Timendus test suite | Timendus suite (optional) |
| Game Boy | Pan Docs | Mooneye acceptance tests (optional) |
| NES | NESdev Wiki | nestest + PPU tests (optional) |
| GBA | GBATEK | mGBA suite (optional) |
| SNES | Anomie's docs / Fullsnes | course fixtures |
| PS1 | PSX-SPX | ps1-tests (optional) |
| PS1 (narrative) | Simias' PSX Guide | ps1-tests |
| NES (companion reading) | Yi Zhang's NES emulator series; Bryan Lee, *Writing an NES Emulator* | — |

External suites are gated by `requires_rom` in grade manifests: supply your
own legal dumps into `roms/` to enable them; everything mandatory in the
course runs on committed course-original fixtures alone.

## Strict-C17 prerequisite track

`docs/CURRICULUM-foundations-c17.md` describes a companion course with a
different contract: **strict ISO C17 cores, stable C ABI boundaries**,
one-concept-per-chapter granularity, CHIP-8 → 6502 → NES only. This lab
intentionally uses C++20 and broader scope; students who want the
C17-discipline path can run that course first — the emulation concepts
transfer one-to-one.

## NES → PS1 concept mapping

| NES | PS1 |
|---|---|
| 2A03 / 6502 | MIPS R3000A |
| CPU bus / MMIO | PS1 bus / MMIO |
| PPU | GPU / VRAM / rasterizer |
| APU | SPU |
| OAM DMA | multi-channel DMA controller |
| NMI/IRQ lines | interrupt controller + device IRQs |
| cartridge | CD-ROM subsystem |
| controller port | SIO / memory cards |
| CPU/PPU/APU scheduling | CPU/GPU/SPU/DMA/CD/timer scheduling |

## Explicitly out of scope before the PS1 phase

Do not block progression on: every mapper beyond the taught set, perfect
commercial-game compatibility, cycle-exact PPU races, DMC sample-loop
accuracy, rewind/run-ahead, netplay, shaders, GUI polish — or the GB/GBA/SNES
phases if you are heading straight for the PlayStation capstone. Those are
depth branches, not gates.
