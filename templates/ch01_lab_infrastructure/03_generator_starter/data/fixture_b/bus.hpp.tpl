// fixture_b — checkpoints 3 and 4 live in a second file; the generator must
// keep per-file numbering and copy this header's text around blocks intact.
#pragma once

#include <cstdint>

namespace fixture_b {

constexpr uint16_t kMagic = 0xB105;

// Text between blocks is copied verbatim in position.

//%LABS-BEGIN 3
//%LABS-SOLUTION
inline uint16_t swap16(uint16_t v) {
    return uint16_t(uint16_t(v << 8) | uint16_t(v >> 8));
}
//%LABS-STUB
inline uint16_t swap16(uint16_t v) {
    (void)v;
    return 0;  // TODO(3): byte-swap a 16-bit value
}
//%LABS-END

// Another verbatim gap, still between blocks.

//%LABS-BEGIN 4
//%LABS-SOLUTION
inline bool has_magic(const uint8_t* p) {
    return p[0] == 0x0B && p[1] == 0xB1;
}
//%LABS-STUB
inline bool has_magic(const uint8_t* p) {
    (void)p;
    return false;  // TODO(4): compare little-endian read at p against kMagic
}
//%LABS-END

}  // namespace fixture_b
