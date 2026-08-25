#pragma once
// ch01/a — mini fixture: one little-endian load (challenge target 'ch01/a')

#include <cstdint>

namespace mini_a {

//%LABS-BEGIN 1
//%LABS-SOLUTION
inline uint16_t read_le16(const uint8_t* p) {
    return uint16_t(uint16_t(p[0]) | uint16_t(p[1]) << 8);
}
//%LABS-STUB
inline uint16_t read_le16(const uint8_t* p) {
    (void)p;
    return 0;  // TODO(1): assemble little-endian 16-bit value
}
//%LABS-END

}  // namespace mini_a
