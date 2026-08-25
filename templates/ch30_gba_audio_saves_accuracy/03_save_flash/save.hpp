#pragma once
// GBA cartridge saves: SRAM byte array plus a state-exact flash chip with
// ID / erase / program / bank commands.
#include <cstdint>
#include <cstring>

namespace gba {

using u8 = uint8_t;
using s8 = int8_t;
using u16 = uint16_t;
using s16 = int16_t;
using u32 = uint32_t;
using s32 = int32_t;
using u64 = uint64_t;

// Synthetic device IDs for this lab's flash (no real vendor's ROM needed).
constexpr u8 kFlashMfgId = 0xE0;
constexpr u8 kFlashDevId64K = 0x51;
constexpr u8 kFlashDevId128K = 0x52;

constexpr u32 kSramSize = 0x8000;      // 32 KiB
constexpr u32 kFlash64K = 0x10000;
constexpr u32 kFlash128K = 0x20000;
constexpr u32 kFlashSector = 0x1000;

struct Sram {
    u8 data[kSramSize] = {};

    u8 read(u32 addr) const { return data[addr % kSramSize]; }
    void write(u32 addr, u8 v) { data[addr % kSramSize] = v; }
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
struct FlashChip {
    enum class State {
        Ready,
        IdMode,
        EraseArmed,     // saw AA 55 80, waiting for confirm
        ProgramPending, // saw AA 55 A0, next write programs
        BankPending,    // saw AA 55 B0, next write selects bank
    };

    u8 mem[kFlash128K] = {};
    bool is128k = false;
    u8 bank = 0;
    State state = State::Ready;
    int prefix = 0;  // progress through the AA 55 two-byte command prefix

    explicit FlashChip(bool size128k) : is128k(size128k) {
        std::memset(mem, 0xFF, sizeof(mem));
    }

    // Feed one write into the command machine. Returns true when the write
    // was consumed as data (program/bank), false when treated as a command.
    bool write(u32 addr, u8 value);

    u8 read(u32 addr) const {
        if (state == State::IdMode) {
            if ((addr & 1) == 0) return kFlashMfgId;
            return is128k ? kFlashDevId128K : kFlashDevId64K;
        }
        u32 off = (u32(bank) << 16) | (addr & 0x1FFFF);
        if (!is128k) off &= 0xFFFF;
        return mem[off];
    }
};

inline bool FlashChip::write(u32 addr, u8 value) {
    switch (state) {
        case State::ProgramPending:
            state = State::Ready;
            prefix = 0;
            {
                u32 off = (u32(bank) << 16) |
                          (addr & (is128k ? 0x1FFFFu : 0xFFFFu));
                mem[off] &= value;  // flash can only clear bits until erase
            }
            return true;
        case State::BankPending:
            state = State::Ready;
            prefix = 0;
            if (is128k && value <= 1) bank = value;
            return true;
        default:
            break;
    }

    // Two-byte AA/55 command prefixes; EraseArmed survives them until the
    // confirming opcode arrives.
    if (prefix == 0) {
        if (value == 0xAA) prefix = 1;
        return false;
    }
    if (prefix == 1) {
        if (value == 0x55)
            prefix = 2;
        else
            prefix = 0;
        return false;
    }
    prefix = 0;
    if (state == State::EraseArmed) {
        if (value == 0x10) {  // chip erase confirm
            std::memset(mem, 0xFF, sizeof(mem));
            state = State::Ready;
        } else if (value == 0x30) {  // sector erase confirm @addr
            u32 base =
                (u32(bank) << 16) |
                (addr & ~(kFlashSector - 1) & (is128k ? 0x1FFFFu : 0xFFFFu));
            std::memset(mem + base, 0xFF, kFlashSector);
            state = State::Ready;
        }
        // anything else leaves erase armed state silently dropped
        state = State::Ready;
        return false;
    }
    switch (value) {
        case 0x90: state = State::IdMode; break;
        case 0xF0: state = State::Ready; break;
        case 0x80: state = State::EraseArmed; break;
        case 0xA0: state = State::ProgramPending; break;
        case 0xB0: state = State::BankPending; break;
        default: break;
    }
    return false;
}
//@LABS-STUB
struct FlashChip {
    enum class State { Ready };
    u8 mem[kFlash128K] = {};
    bool is128k = false;
    u8 bank = 0;
    State state = State::Ready;

    explicit FlashChip(bool size128k) : is128k(size128k) {
        std::memset(mem, 0xFF, sizeof(mem));
    }

    // TODO(1): full command machine — see DEBUGGING-free spec in SPEC.md.
    bool write(u32, u8) { return false; }              // TODO(1)
    u8 read(u32 addr) const {                          // TODO(1)
        return mem[addr & (is128k ? 0x1FFFFu : 0xFFFFu)];
    }
};
//@LABS-END

}  // namespace gba
