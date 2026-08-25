#pragma once
#include <cstdint>
#include <string>

namespace chip8 {

// FNV-1a 64-bit, the course-wide frame digest. Identical algorithm to
// tools/labs/hash_frame.py so C++ and manifest hashes always agree.
inline uint64_t fnv1a64(const uint8_t* data, std::size_t n) {
    uint64_t h = 0xCBF29CE484222325ULL;
    for (std::size_t i = 0; i < n; ++i) {
        h ^= data[i];
        h *= 0x100000001B3ULL;
    }
    return h;
}

inline std::string fnv1a64_hex(const uint8_t* data, std::size_t n) {
    char buf[17];
    const uint64_t h = fnv1a64(data, n);
    for (int k = 0; k < 16; ++k)
        buf[k] = "0123456789ABCDEF"[(h >> (60 - 4 * k)) & 0xF];
    return std::string(buf, 16);
}

}  // namespace chip8
