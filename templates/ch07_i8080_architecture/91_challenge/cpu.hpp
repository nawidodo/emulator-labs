#pragma once
#include <cstdint>
#include <string>

#include "alu.hpp"
#include "flags.hpp"

// Intel 8080 CPU core — chapter 7 subset.
//
// Supported instruction groups (the chapter 7 contract):
//   MOV / MVI / LXI / LDA / STA / INR / DCR
//   ADD / ADC / SUB / SBB / ANA / XRA / ORA / CMP   (register + immediate)
//   NOP, HLT
//
// Control flow, stack and interrupts arrive in chapter 8; I/O ports in
// chapter 9. step() returns the exact T-state cost of the executed
// instruction so timing is testable (curriculum §56).

namespace i8080 {

class Bus {
public:
    virtual ~Bus() = default;
    virtual uint8_t read(uint16_t addr) const = 0;
    virtual void write(uint16_t addr, uint8_t val) = 0;
};

// Simple 64 KiB flat memory for tests and small fixtures.
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
    bool halted = false;

    Bus* bus = nullptr;      // never owned by the Cpu
    uint64_t cycles = 0;     // cumulative T-state count

    void reset();

    // Execute one instruction; returns its T-state cost.
    uint64_t step();

    // Register-pair views (BC, DE, HL order matches opcode encoding).
    uint16_t bc() const { return uint16_t(b) << 8 | c; }
    uint16_t de() const { return uint16_t(d) << 8 | e; }
    uint16_t hl() const { return uint16_t(h) << 8 | l; }
    void set_bc(uint16_t v) { b = uint8_t(v >> 8); c = uint8_t(v); }
    void set_de(uint16_t v) { d = uint8_t(v >> 8); e = uint8_t(v); }
    void set_hl(uint16_t v) { h = uint8_t(v >> 8); l = uint8_t(v); }

    // Merge S/Z/P of an 8-bit result into the flag set.
    void set_szp(uint8_t v);

    std::string disassemble(uint16_t at) const;
};

}  // namespace i8080
