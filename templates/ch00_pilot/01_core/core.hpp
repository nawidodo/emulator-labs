#pragma once
#include <cstdint>
#include <span>

namespace pilot {

struct Cpu {
    uint8_t a = 0;
    uint8_t pc = 0;

    void reset();
    void load(std::span<const uint8_t> rom);
    int step();
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline uint16_t read_le16(const uint8_t* p) {
    return uint16_t(p[0]) | uint16_t(p[1]) << 8;
}
//@LABS-STUB
// TODO(1): implement little-endian 16-bit read (low byte first).
// Stub compiles so the suite runs RED until you finish it.
inline uint16_t read_le16(const uint8_t* p) {
    (void)p;
    return 0;  // wrong on purpose
}
//@LABS-END

}  // namespace pilot
