#pragma once
// Challenge scene container (.sns) — a deterministic snapshot of everything
// this chapter's renderer consumes.
//
// Layout (little-endian throughout):
//
//   offset  size    field
//   0x00    8       magic "SNESSCN1"
//   0x08    2       u16 version (currently 1)
//   0x0A    2       u16 header_size (currently 80; data follows the header)
//   0x0C    4       reserved (zero)
//   0x10    64      register block, see SceneRegs below
//   0x50    65536   VRAM image, word-addressed little-endian words
//   0x10050 512     CGRAM, 256 BGR555 entries
//   0x10250 2048    OAM low table (512 sprites x 4 bytes)
//   0x10A50 128     OAM high table (2 bits per sprite)
//
// Total file size is always 68304 bytes.
//
// Register block:
//
//   off   size  field
//   0x00  2     u16 mode: 0, 1 or 7
//   0x02  2     u16 flags: bit1 = Mode 7 wrap (else backdrop out-of-range)
//   0x04  12    u16 bg{1..3} tile_base, map_base (VRAM word addresses)
//   0x24  4     u16 Mode 7 center x0,y0 (13-bit signed)
//   0x28  4     u16 Mode 7 hofs,vofs
//   0x2C  1     u8  window_left (inclusive)
//   0x2D  1     u8  window_right (inclusive)
//   0x2E  1     u8  window_flags: bit0 enable, bit1 invert,
//                   bits 2-5 layer_mask (bit2 = BG1 .. bit5 = BG4),
//                   bit6 color-math enable inside window
//   0x2F  1     u8  color_math_flags: bit0 op (0=add 1=sub), bit1 half,
//                   bit2 color math enabled
//   0x30  16    reserved (zero)

#include <array>
#include <cstdint>
#include <cstring>
#include <string>

namespace snesbus {

struct SceneRegs {
    uint16_t mode = 1;
    bool m7_wrap = false;
    uint16_t tile_base[3] = {0, 0, 0};
    uint16_t map_base[3] = {0, 0, 0};
    uint16_t hofs[3] = {0, 0, 0};
    uint16_t vofs[3] = {0, 0, 0};
    int16_t m7_a = 0x0100;
    int16_t m7_b = 0;
    int16_t m7_c = 0;
    int16_t m7_d = 0x0100;
    uint16_t m7_x0 = 0;
    uint16_t m7_y0 = 0;
    uint16_t m7_hofs = 0;
    uint16_t m7_vofs = 0;
    uint8_t win_left = 0;
    uint8_t win_right = 255;
    bool win_enable = false;
    bool win_invert = false;
    uint8_t win_layer_mask = 0xF;
    bool win_cmath_enable = false;
    bool cmath_sub = false;
    bool cmath_half = false;
    bool cmath_enable = false;
};

struct Scene {
    SceneRegs regs;
    std::array<uint16_t, 32768> vram{};
    std::array<uint16_t, 256> cgram{};
    std::array<uint8_t, 2048> oam_low{};  // parsed for completeness/inspection
    std::array<uint8_t, 128> oam_high{};
};

inline constexpr size_t kSceneHeaderSize = 80;
inline constexpr size_t kSceneVramOff = kSceneHeaderSize;
inline constexpr size_t kSceneCgramOff = kSceneVramOff + 65536;
inline constexpr size_t kSceneOamLowOff = kSceneCgramOff + 512;
inline constexpr size_t kSceneOamHighOff = kSceneOamLowOff + 2048;
inline constexpr size_t kSceneSize = kSceneOamHighOff + 128;

inline uint16_t rd16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

// Returns "" on success, otherwise a human-readable reason.
inline std::string load_scene(const uint8_t* data, size_t len, Scene* out) {
    if (len != kSceneSize) {
        return std::string("bad size ") + std::to_string(len) + ", want " +
               std::to_string(kSceneSize);
    }
    static const char kMagic[8] = {'S', 'N', 'E', 'S', 'S', 'C', 'N', '1'};
    if (std::memcmp(data, kMagic, 8) != 0) {
        return "bad magic";
    }
    if (rd16(data + 0x08) != 1) {
        return "unsupported version";
    }
    if (rd16(data + 0x0A) != kSceneHeaderSize) {
        return "unsupported header size";
    }

    Scene& s = *out;
    const uint8_t* r = data + 0x10;
    SceneRegs& g = s.regs;
    g.mode = rd16(r + 0x00);
    g.m7_wrap = (rd16(r + 0x02) & 0x2u) != 0;
    for (int i = 0; i < 3; ++i) {
        g.tile_base[i] = rd16(r + 0x04 + i * 4u);
        g.map_base[i] = rd16(r + 0x06 + i * 4u);
        g.hofs[i] = rd16(r + 0x10 + i * 4u);
        g.vofs[i] = rd16(r + 0x12 + i * 4u);
    }
    auto rs16 = [](const uint8_t* p) {
        return static_cast<int16_t>(rd16(p));
    };
    g.m7_a = rs16(r + 0x1C);
    g.m7_b = rs16(r + 0x1E);
    g.m7_c = rs16(r + 0x20);
    g.m7_d = rs16(r + 0x22);
    g.m7_x0 = rd16(r + 0x24);
    g.m7_y0 = rd16(r + 0x26);
    g.m7_hofs = rd16(r + 0x28);
    g.m7_vofs = rd16(r + 0x2A);
    g.win_left = r[0x2C];
    g.win_right = r[0x2D];
    const uint8_t wf = r[0x2E];
    g.win_enable = (wf & 0x01u) != 0;
    g.win_invert = (wf & 0x02u) != 0;
    g.win_layer_mask = static_cast<uint8_t>((wf >> 2) & 0xFu);
    g.win_cmath_enable = (wf & 0x40u) != 0;
    const uint8_t cf = r[0x2F];
    g.cmath_sub = (cf & 0x01u) != 0;
    g.cmath_half = (cf & 0x02u) != 0;
    g.cmath_enable = (cf & 0x04u) != 0;

    // VRAM words are little-endian pairs of file bytes.
    for (unsigned i = 0; i < 32768; ++i) {
        s.vram[i] = rd16(data + kSceneVramOff + i * 2u);
    }
    for (unsigned i = 0; i < 256; ++i) {
        s.cgram[i] = rd16(data + kSceneCgramOff + i * 2u);
    }
    std::memcpy(s.oam_low.data(), data + kSceneOamLowOff, 2048);
    std::memcpy(s.oam_high.data(), data + kSceneOamHighOff, 128);
    return "";
}

}  // namespace snesbus
