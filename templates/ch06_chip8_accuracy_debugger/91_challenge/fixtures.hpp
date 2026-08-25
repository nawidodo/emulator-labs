#pragma once
// Synthetic quirk fixtures for the ch06 challenge matrix. Each fixture is
// hand-assembled (see the .asm.txt files under
// tests/public/ch06_chip8_accuracy_debugger/fixtures/) and produces a
// DIFFERENT final register state depending on which historical quirk is
// active — so a profile matrix over them pins down every flag.

#include <cstdint>

namespace ch06 {

inline constexpr uint8_t kQ1Shift[] = {
    0x62, 0xF0,  // 200: LD  V2, 0xF0
    0x63, 0x01,  // 202: LD  V3, 0x01
    0x82, 0x36,  // 204: SHR V2, {V3|V2}
    0x84, 0x20,  // 206: LD  V4, V2
};

inline constexpr uint8_t kQ2LoadStore[] = {
    0x63, 0x12,  // 200: LD  V3, 0x12
    0xA4, 0x00,  // 202: LD  I, 0x400
    0xF3, 0x55,  // 204: LD  [I], V3
    0xF3, 0x65,  // 206: LD  V3, [I]
};

inline constexpr uint8_t kQ3Jump[] = {
    0x60, 0x05,  // 200: LD  V0, 0x05
    0x62, 0x03,  // 202: LD  V2, 0x03
    0xB2, 0x05,  // 204: JP  205 + {V0|V2}
    0x00, 0x00,  // 206: NOP (padding)
    0x12, 0x14,  // 208: JP  214   <- CHIP48 lands here
    0x64, 0x11,  // 20A: LD  V4, 0x11
    0x12, 0x14,  // 20C: JP  214
    0x00, 0x00,  // 20E: NOP (padding)
};

inline constexpr uint8_t kQ4DrawWrap[] = {
    0x60, 0xAA,  // 200: LD   V0, 0xAA \ sprite row bytes
    0x61, 0xAA,  // 202: LD   V1, 0xAA /
    0xA4, 0x00,  // 204: LD   I, 0x400
    0xF1, 0x55,  // 206: LD   [I], V1
    0x60, 0x00,  // 208: LD   V0, 0
    0x61, 0x00,  // 20A: LD   V1, 0
    0xA4, 0x00,  // 20C: LD   I, 0x400
    0xD0, 0x12,  // 20E: DRW  V0, V1, 2
    0x60, 0x3E,  // 210: LD   V0, 62
    0xA4, 0x00,  // 212: LD   I, 0x400
    0xD0, 0x12,  // 214: DRW  V0, V1, 2
};

inline constexpr uint8_t kQ5VfReset[] = {
    0x64, 0xFF,  // 200: LD   V4, 0xFF
    0xA4, 0x00,  // 202: LD   I, 0x400
    0xF4, 0x1E,  // 204: ADD  I, V4
    0x65, 0xFF,  // 206: LD   V5, 0xFF
    0x66, 0x11,  // 208: LD   V6, 0x11
    0x86, 0x44,  // 20A: ADD  V6, V4
    0xA4, 0x00,  // 20C: LD   I, 0x400
    0xF4, 0x1E,  // 20E: ADD  I, V4
};

}  // namespace ch06
