#pragma once
#include <cstdint>
#include <cstddef>
#include <span>
#include <string>

// FNV-1a 64-bit, identical to tools/labs/grade.py and tools/labs/hash_frame.py.
// Every golden digest in this course is an FNV-1a 64 hash over raw bytes, so
// tests can pin traces, VRAM images and PCM buffers without a runtime
// dependency on the python tooling.
namespace psxmini {

constexpr uint64_t kFnvOffset = 0xCBF29CE484222325ULL;
constexpr uint64_t kFnvPrime  = 0x100000001B3ULL;

inline uint64_t fnv64(std::span<const uint8_t> data) {
    uint64_t h = kFnvOffset;
    for (uint8_t b : data) {
        h ^= b;
        h *= kFnvPrime;
    }
    return h;
}

inline uint64_t fnv64(const std::string& text) {
    auto* p = reinterpret_cast<const uint8_t*>(text.data());
    return fnv64({p, text.size()});
}

}  // namespace psxmini
