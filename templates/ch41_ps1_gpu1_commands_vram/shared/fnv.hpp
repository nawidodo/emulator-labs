#pragma once
// FNV-1a 64-bit — identical algorithm and parameters as tools/labs/grade.py
// (offset basis 0xCBF29CE484222325, prime 0x100000001B3, "%016X" uppercase).
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>

namespace psx::gpu {

inline std::string fnv64_hex(std::span<const uint8_t> data) {
    uint64_t h = 0xCBF29CE484222325ull;
    for (uint8_t b : data) {
        h ^= b;
        h *= 0x100000001B3ull;
    }
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llX",
                  static_cast<unsigned long long>(h));
    return std::string(buf);
}

}  // namespace psx::gpu
