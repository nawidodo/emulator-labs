#pragma once
#include <cstdint>
#include <array>

#include "psx_mini.hpp"

// The chapter's synthetic CPU fixture: sums 5+4+3+2+1 into r1, stores the
// result at RAM byte 64, halts. 7 instructions, 15 executed lines of trace.
// Committed byte-for-byte as tests/public/ch50_ps1_accuracy_trace_testing/
// fixtures/cpu_program.bin with the listing in cpu_program.asm.txt.
namespace psxmini {

inline constexpr std::array<uint32_t, 7> kCpuProgram = {
    enc(kOpLi, 1, 0, 0, 0),      // 0x00  li   r1, 0
    enc(kOpLi, 2, 0, 0, 5),      // 0x04  li   r2, 5
    enc(kOpAdd, 1, 1, 2, 0),     // 0x08  loop: add r1, r1, r2
    enc(kOpAddi, 2, 2, 0, 0xFFF),// 0x0C  addi r2, r2, -1
    enc(kOpBne, 0, 2, 0, 0xFFD), // 0x10  bne  r2, r0, loop   (-3 words)
    enc(kOpSw, 0, 0, 1, 0x40),   // 0x14  sw   r1, 64(r0)  (rt = data reg)
    enc(kOpHalt, 0, 0, 0, 0),    // 0x18  halt
};

}  // namespace psxmini
