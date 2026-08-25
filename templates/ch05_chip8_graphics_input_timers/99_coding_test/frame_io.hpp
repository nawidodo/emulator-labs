#pragma once
#include "scroll_machine.hpp"
#include "fnv.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace chip8 {

inline constexpr std::size_t kRgbaFrameBytes =
    std::size_t(kWidth) * kHeight * 4;

// Expands the 1-bit framebuffer into RGBA8888, row-major, one 4-byte pixel
// per display pixel. Convention frozen course-wide:
//   pixel ON  -> FF FF FF FF (white)
//   pixel OFF -> 00 00 00 00 (black)
// `out` must hold kRgbaFrameBytes.
//@LABS-BEGIN 1
//@LABS-SOLUTION
inline void expand_rgba(const Display& d, uint8_t* out) {
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            uint8_t* px = out + (std::size_t(y) * kWidth + x) * 4;
            const bool on = d.pixels[y * kWidth + x];
            px[0] = on ? 0xFF : 0x00;  // R
            px[1] = on ? 0xFF : 0x00;  // G
            px[2] = on ? 0xFF : 0x00;  // B
            px[3] = on ? 0xFF : 0x00;  // A
        }
    }
}
//@LABS-STUB
// TODO(1): expand the 64x32 monochrome framebuffer into RGBA8888 bytes.
// Pixel convention: ON -> FF FF FF FF, OFF -> 00 00 00 00.
inline void expand_rgba(const Display& /*d*/, uint8_t* /*out*/) {}
//@LABS-END

inline std::vector<uint8_t> frame_bytes(const Display& d) {
    std::vector<uint8_t> buf(kRgbaFrameBytes);
    expand_rgba(d, buf.data());
    return buf;
}

// The digest manifests compare: FNV-1a 64 over the RGBA8888 expansion.
inline std::string frame_hash_hex(const Display& d) {
    const std::vector<uint8_t> buf = frame_bytes(d);
    return fnv1a64_hex(buf.data(), buf.size());
}

inline bool write_byte_file(const std::string& path,
                            const uint8_t* data, std::size_t n) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const std::size_t written = std::fwrite(data, 1, n, f);
    std::fclose(f);
    return written == n;
}

inline bool write_text_file(const std::string& path, const std::string& s) {
    return write_byte_file(path, reinterpret_cast<const uint8_t*>(s.data()),
                           s.size());
}

}  // namespace chip8
