#pragma once
// fx8 — the fictional 8-bit CPU introduced in ch02 (fictional core).
// Verbatim copy kept self-contained so this chapter builds standalone.
//
//   256-byte memory, program loaded at $00
//   regs a, x, y; flags z (zero), c (carry); pc 8-bit
//   all instructions are 1 byte + inline operands
//
//   0x00 NOP            1 cycle
//   0x01 LDA #imm       2
//   0x02 LDA addr       3
//   0x03 STA addr       3
//   0x04 ADD #imm       2   sets z,c(carry out)
//   0x05 ADD addr       3
//   0x06 SUB #imm       2   sets z,c(no borrow)
//   0x07 JMP addr       2
//   0x08 JZ addr        2 (3 when taken)
//   0x0B OUT            2   writes A to output port via on_out
//   0xFF HALT           1

#include <array>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <span>

namespace fx8 {

struct Cpu {
    uint8_t a = 0;
    uint8_t x = 0;
    uint8_t y = 0;
    uint8_t pc = 0;
    bool z = false;
    bool c = false;
    bool halted = false;
    std::array<uint8_t, 256> mem{};

    // Cumulative guest cycles executed by this CPU (device-local ledger,
    // derived from steps — NOT an independent notion of "now").
    uint64_t cycles = 0;

    // Port hook: the SoC wires OUT to the UART device.
    std::function<void(uint8_t)> on_out;

    void reset() {
        a = x = y = 0;
        pc = 0;
        z = c = false;
        halted = false;
        cycles = 0;
    }

    void load(std::span<const uint8_t> rom) {
        for (size_t i = 0; i < rom.size() && i < mem.size(); ++i)
            mem[i] = rom[i];
    }

    // Execute one instruction; returns cycles consumed.
    int step() {
        if (halted) return 0;
        const uint8_t op_pc = pc;
        const uint8_t op = mem[pc++];
        int cost = 1;
        auto imm = [&] { return mem[pc++]; };
        switch (op) {
            case 0x00:
                cost = 1;
                break;
            case 0x01:
                a = imm();
                z = a == 0;
                cost = 2;
                break;
            case 0x02:
                a = mem[imm()];
                z = a == 0;
                cost = 3;
                break;
            case 0x03:
                mem[imm()] = a;
                cost = 3;
                break;
            case 0x04: {
                const uint16_t s = uint16_t(a) + imm();
                a = uint8_t(s);
                c = s > 0xFF;
                z = a == 0;
                cost = 2;
                break;
            }
            case 0x05: {
                const uint16_t s = uint16_t(a) + mem[imm()];
                a = uint8_t(s);
                c = s > 0xFF;
                z = a == 0;
                cost = 3;
                break;
            }
            case 0x06: {
                const int16_t d = int16_t(a) - imm();
                a = uint8_t(d);
                c = d >= 0;
                z = a == 0;
                cost = 2;
                break;
            }
            case 0x07:
                pc = imm();
                cost = 2;
                break;
            case 0x08: {
                const uint8_t t = imm();
                if (z) {
                    pc = t;
                    cost = 3;
                } else {
                    cost = 2;
                }
                break;
            }
            case 0x0B:
                if (on_out) on_out(a);
                cost = 2;
                break;
            case 0xFF:
                halted = true;
                cost = 1;
                break;
            default:
                // Undefined opcode: treat as HALT so traces terminate.
                halted = true;
                cost = 1;
                break;
        }
        cycles += uint64_t(cost);
        last_pc_ = op_pc;
        last_op_ = op;
        return cost;
    }

    // Info about the most recent step(), for trace logging.
    uint8_t last_pc() const { return last_pc_; }
    uint8_t last_op() const { return last_op_; }

private:
    uint8_t last_pc_ = 0;
    uint8_t last_op_ = 0;
};

}  // namespace fx8
