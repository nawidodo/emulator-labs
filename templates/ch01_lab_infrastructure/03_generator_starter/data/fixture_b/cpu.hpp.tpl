// fixture_b — four checkpoints across two files (C++ headers)
#pragma once

#include <cstdint>

namespace fixture_b {

//%LABS-BEGIN 1
//%LABS-SOLUTION
inline uint8_t lo_byte(uint16_t v) { return uint8_t(v & 0xFFu); }
//%LABS-STUB
inline uint8_t lo_byte(uint16_t v) {
    (void)v;
    return 0;  // TODO(1): low byte of v
}
//%LABS-END

//%LABS-BEGIN 2
//%LABS-SOLUTION
inline uint8_t hi_byte(uint16_t v) { return uint8_t(v >> 8); }
//%LABS-STUB
inline uint8_t hi_byte(uint16_t v) {
    (void)v;
    return 0;  // TODO(2): high byte of v
}
//%LABS-END

}  // namespace fixture_b
