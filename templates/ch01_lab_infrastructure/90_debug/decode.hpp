#pragma once
// ch01/90_debug — intentionally broken endian decoding.
//
// The skeleton (STUB) side of each block below carries a SEEDED BUG; the
// solution side is the corrected code. Reproduce the failures with the
// test suite, minimize, diagnose, fix — then write bug-report.md
// (see DEBUGGING.md).
//
// Context: read_le16 below is CORRECT and stays fixed for reference.

#include <cstdint>

namespace ch01_debug {

inline uint16_t read_le16(const uint8_t* p) {
    return uint16_t(uint16_t(p[0]) | uint16_t(p[1]) << 8);
}

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline uint32_t read_le32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 |
           uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
//@LABS-STUB
// BUG 1: this decoder assembles the four bytes in the wrong ORDER.
// Symptom: every little-endian 32-bit field comes out byte-reversed
// (0x12345678 reads back as 0x78563412), so header magic checks fail
// everywhere while 16-bit fields look fine.
inline uint32_t read_le32(const uint8_t* p) {
    // TODO(1): diagnose why 32-bit little-endian fields are reversed.
    return uint32_t(p[3]) | uint32_t(p[2]) << 8 |
           uint32_t(p[1]) << 16 | uint32_t(p[0]) << 24;
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
inline uint16_t read_be16(const uint8_t* p) {
    return uint16_t(uint16_t(p[0]) << 8 | uint16_t(p[1]));
}
//@LABS-STUB
// BUG 2: this decoder starts one byte too late.
// Symptom: big-endian 16-bit values are garbled (each result combines the
// wrong pair of bytes), and the read touches one byte PAST the logical
// field — harmless mid-buffer, a crash waiting at buffer ends.
// NOTE: tests only ever hand this function buffers with one spare trailing
// byte so the buggy read stays in-bounds; do not "fix" the tests.
inline uint16_t read_be16(const uint8_t* p) {
    // TODO(2): diagnose the off-by-one in the big-endian assembly.
    return uint16_t(uint16_t(p[1]) << 8 | uint16_t(p[2]));
}
//@LABS-END

}  // namespace ch01_debug
