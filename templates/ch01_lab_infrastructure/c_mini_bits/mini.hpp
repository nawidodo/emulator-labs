#pragma once
// ch01/c — mini fixture: bit-field extraction (challenge target 'ch01/c')

#include <cstdint>

namespace mini_c {

//@LABS-BEGIN 1
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
    return 0;  // TODO(1): shift right by start, mask to count bits
}
//@LABS-END

}  // namespace mini_c
