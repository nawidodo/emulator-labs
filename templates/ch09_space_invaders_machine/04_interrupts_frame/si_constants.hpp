#pragma once
#include <cstdint>

// Board-wide constants shared by the chapter 9 exercises.
//
// The documented timing model: a fixed 1920 kHz machine clock, chosen so
// one 60 Hz frame is EXACTLY 32000 T-states (1920000 / 60). Historical
// boards ran ~2 MHz with sloppy vertical timing; we trade authenticity
// for determinism (see SPEC.md).
//
// Interrupt opcodes jammed onto the bus by the dual one-shot vblank
// timers: RST 08 on even frames, RST 10 on odd frames.

namespace si {

constexpr int kScreenWidth    = 224;                 // upright orientation
constexpr int kScreenHeight   = 256;
constexpr uint32_t kClockKHz  = 1920;
constexpr uint64_t kCyclesPerFrame = kClockKHz * 1000ull / 60ull;  // 32000

constexpr uint8_t kIrqOpcodeEven = 0xCF;   // RST 08 -> vector 0x0008
constexpr uint8_t kIrqOpcodeOdd  = 0xD7;   // RST 10 -> vector 0x0010

}  // namespace si
