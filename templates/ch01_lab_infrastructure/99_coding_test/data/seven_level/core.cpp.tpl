// seven_level — core fixture: checkpoints 1, 3, 5, 7 ('//'-prefixed blocks)
// The odd-numbered checkpoints interleave with util.py.tpl's even ones, so
// a correct generator must track per-file sequences independently.
#include <cstdint>

namespace seven {

//%LABS-BEGIN 1
//%LABS-SOLUTION
inline uint16_t lo16(uint32_t v) { return uint16_t(v & 0xFFFFu); }
//%LABS-STUB
inline uint16_t lo16(uint32_t v) {
    (void)v;
    return 0;  // TODO(1): return the low 16 bits of v
}
//%LABS-END

//%LABS-BEGIN 3
//%LABS-SOLUTION
inline uint16_t hi16(uint32_t v) { return uint16_t(v >> 16); }
//%LABS-STUB
inline uint16_t hi16(uint32_t v) {
    (void)v;
    return 0;  // TODO(3): return the high 16 bits of v
}
//%LABS-END

//%LABS-BEGIN 5
//%LABS-SOLUTION
inline uint16_t swap16(uint16_t v) {
    return uint16_t(uint16_t(v << 8) | uint16_t(v >> 8));
}
//%LABS-STUB
inline uint16_t swap16(uint16_t v) {
    (void)v;
    return 0;  // TODO(5): byte-swap a 16-bit value
}
//%LABS-END

//%LABS-BEGIN 7
//%LABS-SOLUTION
inline uint32_t pack16(uint16_t hi, uint16_t lo) {
    return (uint32_t(hi) << 16) | lo;
}
//%LABS-STUB
inline uint32_t pack16(uint16_t hi, uint16_t lo) {
    (void)hi;
    (void)lo;
    return 0;  // TODO(7): pack two 16-bit halves into one 32-bit word
}
//%LABS-END

}  // namespace seven
