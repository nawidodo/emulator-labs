#pragma once
#include <cstdint>
#include <string>

#include "alu.hpp"
#include "flags.hpp"

// Compact Intel 8080 core for machine-level work (chapter 9).
//
// This is the chapter 8 control-flow core carried forward verbatim and
// extended with the two instructions a MACHINE needs that a CPU-isolated
// chapter does not: IN (DB) and OUT (D3). The Bus interface grows matching
// in()/out() hooks with neutral defaults, so every chapter 7/8 program
// still runs unchanged on a FlatBus.
//
// Timing stays part of the step: step() returns the exact T-state cost,
// and chapter 9 frame cadence arithmetic depends on those numbers.

namespace i8080 {

class Bus {
public:
    virtual ~Bus() = default;
    virtual uint8_t read(uint16_t addr) const = 0;
    virtual void write(uint16_t addr, uint8_t val) = 0;

    // I/O space is decoded by the MACHINE, not the CPU. Defaults keep a
    // bare FlatBus usable: reads float low, writes vanish.
    virtual uint8_t in(uint8_t port) { (void)port; return 0x00; }
    virtual void out(uint8_t port, uint8_t val) { (void)port; (void)val; }
};

struct FlatBus final : Bus {
    uint8_t mem[0x10000] = {};

    uint8_t read(uint16_t addr) const override { return mem[addr]; }
    void write(uint16_t addr, uint8_t val) override { mem[addr] = val; }
};

struct Cpu {
    // Registers.
    uint8_t a = 0, b = 0, c = 0, d = 0, e = 0, h = 0, l = 0;
    uint16_t sp = 0, pc = 0;

    // Flags.
    bool s = false, z = false, p = false, cy = false, ac = false;
    bool iff = true;        // interrupt flip-flop (EI/DI)
    bool halted = false;

    Bus* bus = nullptr;
    uint64_t cycles = 0;

    void reset();

    // Execute one instruction; returns its T-state cost.
    uint64_t step();

    // Hardware interrupt entry: `opcode` is the byte jammed onto the bus
    // during INTA (RST 08 = 0xCF / RST 10 = 0xD7 on Space Invaders).
    // Accepted only when IFF is set; acceptance pushes PC and clears IFF.
    // Returns true when the interrupt was taken.
    bool interrupt(uint8_t opcode);

    uint16_t bc() const { return uint16_t(b) << 8 | c; }
    uint16_t de() const { return uint16_t(d) << 8 | e; }
    uint16_t hl() const { return uint16_t(h) << 8 | l; }
    void set_bc(uint16_t v) { b = uint8_t(v >> 8); c = uint8_t(v); }
    void set_de(uint16_t v) { d = uint8_t(v >> 8); e = uint8_t(v); }
    void set_hl(uint16_t v) { h = uint8_t(v >> 8); l = uint8_t(v); }

    void set_szp(uint8_t v);
};

}  // namespace i8080
