// Vendored reference serializer (ch35/01 solution) for reuse.
#pragma once
// Versioned, field-explicit serialization of chip8::Machine.
//
// Layout (little-endian), schema version 1:
//   [0]      version byte (1)
//   [1..17)  V[16]
//   [17..19) I
//   [19..21) pc
//   [21]     sp
//   [22]     dt
//   [23]     st
//   [24]     wait_key
//   [25..27) keys (u16)
//   [27..31) rng (u32)
//   [31..31+4096)  mem
//   [...+2048)     fb
// Total: 6175 bytes.
#include <cstdint>
#include <span>
#include <vector>

namespace chip8 {

inline constexpr uint8_t kStateVersion = 1;
// Offsets are fixed by the layout above; sizes derive from the machine.
inline constexpr size_t kStateSize =
    31 + 4096 + chip8::kW * chip8::kH;

inline size_t write_state(const Machine& m, std::span<uint8_t> out) {
    if (out.size() < kStateSize) return 0;
    size_t o = 0;
    auto u8 = [&](uint8_t v) { out[o++] = v; };
    auto u16 = [&](uint16_t v) {
        out[o++] = uint8_t(v);
        out[o++] = uint8_t(v >> 8);
    };
    auto u32 = [&](uint32_t v) {
        for (int k = 0; k < 4; ++k) out[o++] = uint8_t(v >> (8 * k));
    };
    u8(kStateVersion);
    for (int r = 0; r < 16; ++r) u8(m.v[size_t(r)]);
    u16(m.i);
    u16(m.pc);
    u8(m.sp);
    u8(m.dt);
    u8(m.st);
    u8(m.wait_key);
    u16(m.keys);
    u32(m.rng);
    for (const uint8_t b : m.mem) u8(b);
    for (const uint8_t b : m.fb) u8(b);
    return o;  // == kStateSize
}

inline bool read_state(std::span<const uint8_t> in, Machine& m) {
    if (in.size() < kStateSize || in[0] != kStateVersion) return false;
    size_t o = 0;
    auto u8 = [&]() { return in[o++]; };
    auto u16 = [&]() {
        const uint16_t lo = in[o++];
        return uint16_t(lo | uint16_t(in[o++] << 8));
    };
    auto u32 = [&]() {
        uint32_t v = 0;
        for (int k = 0; k < 4; ++k) v |= uint32_t(in[o++]) << (8 * k);
        return v;
    };
    if (u8() != kStateVersion) return false;  // unreachable, checked above
    for (int r = 0; r < 16; ++r) m.v[size_t(r)] = u8();
    m.i = u16();
    m.pc = u16();
    m.sp = u8();
    m.dt = u8();
    m.st = u8();
    m.wait_key = u8();
    m.keys = u16();
    m.rng = u32();
    for (uint8_t& b : m.mem) b = u8();
    for (uint8_t& b : m.fb) b = u8();
    return true;
}

// FNV-1a 64 over the serialized blob — THE identity of a machine state.
inline uint64_t state_hash(const Machine& m) {
    std::vector<uint8_t> blob(kStateSize);
    write_state(m, blob);
    uint64_t h = 0xCBF29CE484222325ull;
    for (const uint8_t b : blob) {
        h ^= b;
        h *= 0x100000001B3ull;
    }
    return h;
}

}  // namespace chip8
