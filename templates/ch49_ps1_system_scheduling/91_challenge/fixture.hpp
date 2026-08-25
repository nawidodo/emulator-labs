#pragma once
#include <cstdint>
#include <array>

// The boot_handshake fixture program (see
// tests/public/ch49_ps1_system_scheduling/roms/boot_handshake.bin and
// boot_handshake.asm.txt). Exercises the cd -> spu -> gpu handshake:
// masks INTC lines {9,2,1}, logs milestone 1, enables the SPU sample
// IRQ (line 9 every 768 cycles), kicks a CD read (line 2 at cyc 20000),
// queues a 1024-pixel GP0 command with IRQ-on-idle (line 1), polls
// I_STAT for each completion, acks it, logs milestones 2/3/7, HALT.
namespace ps1sys {

constexpr unsigned kBootHandshakeWords = 33;
inline std::array<uint32_t, kBootHandshakeWords> boot_handshake_rom() {
    return std::array<uint32_t, kBootHandshakeWords>({
        0x00081F80, 0x08090001, 0x000A0000, 0x040A0206,
        0x0D0A0074, 0x080C0001, 0x0D0C0FF0, 0x0D090C00,
        0x0D090800, 0x000D0010, 0x040D0400, 0x0D0D0810,
        0x000E0400, 0x040E0001, 0x0D0E0814, 0x110B0070,
        0x216B0004, 0x15600001, 0x1800000F, 0x0D0B0070,
        0x080C0002, 0x0D0C0FF0, 0x110B0070, 0x216B0002,
        0x15600001, 0x18000016, 0x0D0B0070, 0x080C0003,
        0x0D0C0FF0, 0x0D000C00, 0x080C0007, 0x0D0C0FF0,
        0x1C000000,
    });
}

}  // namespace ps1sys
