#pragma once
// ch01/01_endian_bits — multi-byte loads and bit-field extraction.
//
// Every emulator bus read ultimately reduces to these two operations:
// assemble wide integers from byte pointers in a documented byte order,
// and slice instruction fields out of opcode words. Getting them right,
// with explicit widening at every step, is the foundation for everything
// that follows in later chapters.
//
// Semantics (see SPEC.md):
//   read_le16/read_le32 : little-endian assembly (low byte at lowest address)
//   read_be16           : big-endian assembly (high byte at lowest address)
//   bits(v,start,count) : (v >> start) masked to count bits;
//                         requires start >= 0, count >= 1, start+count <= 32

#include <cstdint>

namespace ch01 {

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline uint16_t read_le16(const uint8_t* p) {
    return uint16_t(uint16_t(p[0]) | uint16_t(p[1]) << 8);
}
//@LABS-STUB
inline uint16_t read_le16(const uint8_t* p) {
    (void)p;
    return 0;  // TODO(1): assemble little-endian 16-bit value (low byte first)
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
inline uint16_t read_be16(const uint8_t* p) {
    return uint16_t(uint16_t(p[0]) << 8 | uint16_t(p[1]));
}
//@LABS-STUB
inline uint16_t read_be16(const uint8_t* p) {
    (void)p;
    return 0;  // TODO(2): assemble big-endian 16-bit value (high byte first)
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
inline uint32_t read_le32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 |
           uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
//@LABS-STUB
inline uint32_t read_le32(const uint8_t* p) {
    (void)p;
    return 0;  // TODO(3): assemble little-endian 32-bit value
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
inline uint32_t bits(uint32_t value, unsigned start, unsigned count) {
    if (count == 32) {
        return value >> start;
    }
    return (value >> start) & ((uint32_t{1} << count) - 1);
}
//@LABS-STUB
inline uint32_t bits(uint32_t value, unsigned start, unsigned count) {
    (void)value;
    (void)start;
    (void)count;
    return 0;  // TODO(4): shift right by start, mask to count bits
}
//@LABS-END

}  // namespace ch01
