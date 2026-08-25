#pragma once
// SNES DMA channel model (chapter 33, exercise 01).
//
// Register map follows Anomie's "SNES hardware register list"
// (https://github.com/gilligan/snesdev/blob/master/docs/snes_registers.txt,
// $43x0-$43xA) and the SNESdev Wiki DMA page
// (https://snes.nesdev.org/wiki/DMA). Ordinary DMA only; the HDMA-indirect
// bit (bit 6 of the control byte) is deliberately ignored here.
#include <cstdint>
#include <span>
#include <vector>

namespace snesdma {

// Full $43x0 control byte layout (Anomie's notation):
//   bit 7    : direction, 0 = A-bus -> B-bus, 1 = B-bus -> A-bus
//   bit 6    : HDMA indirect mode (ignored in this exercise)
//   bit 5    : unused
//   bits 4-3 : A-bus addressing: 00 increment, 01 fixed, 10 decrement,
//              11 fixed
//   bits 2-0 : transfer pattern (mode 0-7)
struct Channel {
    uint8_t control = 0;      // $43x0 DMAPx
    uint8_t b_reg = 0;        // $43x1 BBADx (low byte of the $21xx B-bus reg)
    uint16_t a_addr = 0;      // $43x2-$43x3 A1TxL/A1TxH
    uint8_t a_bank = 0;       // $43x4 A1Bx
    uint16_t unit_count = 0;  // $43x5 DASxL (number of bytes to move)
};

// All DMA B-bus registers live on the $2100-$21FF page.
inline constexpr int kBbusBase = 0x2100;

// One recorded bus transaction: the full B-bus address touched and the
// absolute A-bus address (bank:addr flattened) the byte came from/went to.
// Tests assert the full (b_addr, a_addr) SEQUENCE, so the pattern table and
// the A-step logic are both observable independently.
struct TransferStep {
    uint16_t b_addr = 0;
    uint32_t a_addr = 0;
};

// Canonical transfer-mode table (SNESdev Wiki "DMA", transfer pattern
// field; identical numbers appear in Anomie's register doc):
//
//   mode | units | B-bus offsets within one transfer
//   -----+-------+------------------------------------
//    0   |   1   | +0                      (reg,reg,reg,reg)
//    1   |   2   | +0,+1                   (reg,reg+1)
//    2   |   2   | +0,+0                   (reg,reg)      e.g. CGRAM $2122
//    3   |   4   | +0,+0,+1,+1             e.g. BG scroll pair
//    4   |   4   | +0,+1,+2,+3             window registers
//    5   |   4   | +0,+1,+0,+1             undocumented
//    6   |   2   | +0,+0   (forces A-step decrement, see a_step_kind)
//    7   |   4   | +0,+0,+1,+1 (forces A-step decrement, see a_step_kind)
//
// The pattern only selects B-bus REGISTER addresses; whether the A-bus
// pointer moves is decided separately by control bits 4-3.
//@LABS-BEGIN 1
//@LABS-SOLUTION
inline constexpr int kUnitsPerTransfer[8] = {1, 2, 2, 4, 4, 4, 2, 4};

inline int units_per_transfer(uint8_t control) {
    return kUnitsPerTransfer[control & 7];
}
//@LABS-STUB
// TODO(1): return the canonical unit count for the transfer mode selected
// by control bits 2-0. The table above this function documents all eight
// modes; modes 0-4 are the ones commercial software actually uses.
inline int units_per_transfer(uint8_t /*control*/) {
    return 1;  // wrong on purpose: only mode 0 happens to be right
}
//@LABS-END

// B-bus register OFFSET (relative to $43x1/BBADx) of unit `unit` inside one
// pattern repeat. `unit` is always < units_per_transfer(control).
//@LABS-BEGIN 2
//@LABS-SOLUTION
inline constexpr uint8_t kUnitBOffset[8][4] = {
    {0, 0, 0, 0},  // mode 0
    {0, 1, 0, 0},  // mode 1
    {0, 0, 0, 0},  // mode 2
    {0, 0, 1, 1},  // mode 3
    {0, 1, 2, 3},  // mode 4
    {0, 1, 0, 1},  // mode 5
    {0, 0, 0, 0},  // mode 6
    {0, 0, 1, 1},  // mode 7
};

inline uint8_t unit_b_offset(uint8_t control, int unit) {
    return kUnitBOffset[control & 7][unit];
}
//@LABS-STUB
// TODO(2): return the B-bus register offset for unit `unit` of the selected
// mode. Encode the full eight-mode table from LECTURE.md exactly; guessing
// "always 0" will pass mode 0/2 tests and fail everything else.
inline uint8_t unit_b_offset(uint8_t /*control*/, int /*unit*/) {
    return 0;  // wrong on purpose for most modes
}
//@LABS-END

enum class AStep : uint8_t { Increment, Fixed, Decrement };

// A-bus addressing behaviour for a channel.
//
// Normally decoded from control bits 4-3 (00 inc, x1 fixed, 1y dec /
// fixed). Modes 6 and 7 OVERRIDE the field and always decrement: the two
// undocumented modes exist precisely to give games a decrementing word
// write without spending bits on it (SNESdev Wiki "DMA").
//@LABS-BEGIN 3
//@LABS-SOLUTION
inline AStep a_step_kind(uint8_t control) {
    const int mode = control & 7;
    if (mode == 6 || mode == 7) return AStep::Decrement;
    switch ((control >> 3) & 3) {
        case 0: return AStep::Increment;
        case 2: return AStep::Decrement;
        default: return AStep::Fixed;  // 01 and 11 both mean fixed
    }
}
//@LABS-STUB
// TODO(3): decode the A-bus step from control bits 4-3, and remember that
// modes 6 and 7 FORCE decrement regardless of those bits.
inline AStep a_step_kind(uint8_t /*control*/) {
    return AStep::Fixed;  // wrong on purpose
}
//@LABS-END

// Runs one DMA channel start to finish, byte by byte, and returns the full
// sequence of bus transactions. Reads source bytes from `a_bus`, a flat
// image of the A-bus addressed as (a_bank << 16) | addr; a transfer that
// walks past the end of `a_bus` stops there (partial log), mirroring how a
// real channel halts when its address leaves mapped memory.
//@LABS-BEGIN 4
//@LABS-SOLUTION
inline std::vector<TransferStep> run_channel(const Channel& ch,
                                             std::span<const uint8_t> a_bus) {
    std::vector<TransferStep> log;
    const int upu = units_per_transfer(ch.control);
    uint32_t a = (uint32_t(ch.a_bank) << 16) | ch.a_addr;
    const AStep step = a_step_kind(ch.control);
    log.reserve(ch.unit_count);
    for (uint16_t i = 0; i < ch.unit_count; ++i) {
        if (a >= a_bus.size()) break;  // walked off the A-bus image
        const int unit = i % upu;
        const auto b =
            uint16_t(kBbusBase + ch.b_reg + unit_b_offset(ch.control, unit));
        log.push_back({b, a});
        switch (step) {
            case AStep::Increment: ++a; break;
            case AStep::Decrement: --a; break;
            case AStep::Fixed: break;
        }
    }
    return log;
}
//@LABS-STUB
// TODO(4): perform the byte-by-byte transfer. For every byte i in
// [0, unit_count): record {(0x2100 + b_reg + unit_b_offset(...)), a_addr},
// then apply the A step. Stop early if `a` leaves `a_bus`. Return the log.
inline std::vector<TransferStep> run_channel(const Channel& /*ch*/,
                                             std::span<const uint8_t> /*a_bus*/) {
    return {};  // wrong on purpose: no transfers performed
}
//@LABS-END

}  // namespace snesdma
