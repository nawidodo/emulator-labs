#define LABSTEST_MAIN
#include "labstest.hpp"
#include <cstddef>

#include "mode7.hpp"

using namespace snesbus;

namespace {

void vbyte(Vram& v, size_t byte_addr, uint8_t value) {
    uint16_t& w = v.w[(byte_addr >> 1) & 0x7FFFu];
    w = (byte_addr & 1u)
            ? static_cast<uint16_t>((w & 0x00FFu) | (value << 8))
            : static_cast<uint16_t>((w & 0xFF00u) | value);
}

// Map tile (tx,ty) gets value (tx + ty * 3) % 190 + 1 (never 0, and small
// enough that a map parked at byte $3000 never clobbers tiles < 192).
void build_map(Vram& v, uint16_t map_base) {
    const size_t base = static_cast<size_t>(map_base) * 2u;
    for (unsigned ty = 0; ty < 128; ++ty) {
        for (unsigned tx = 0; tx < 128; ++tx) {
            const uint8_t t = static_cast<uint8_t>(
                (tx + ty * 3u) % 190u + 1u);
            vbyte(v, base + ty * 128u + tx, t);
        }
    }
}

uint32_t pixel_rgba(const std::array<uint8_t, kScreenWidth * kScreenHeight * 4>& f,
                    int x, int y) {
    const size_t o = (static_cast<size_t>(y) * kScreenWidth + x) * 4u;
    return f[o] | (f[o + 1] << 8) | (f[o + 2] << 16) | (f[o + 3] << 24);
}

constexpr size_t kFbBytes = kScreenWidth * kScreenHeight * 4;

}  // namespace

TEST(transform, identity_maps_screen_to_self) {
    Mode7Params p;  // A=D=$0100, B=C=0, centers/offsets zero
    for (int x : {0, 17, 255}) {
        for (int y : {0, 100, 223}) {
            const Mode7Raw r = mode7_transform(p, x, y);
            EXPECT_EQ(r.u, x * 256);
            EXPECT_EQ(r.v, y * 256);
            const MapPos m = to_map_pos(r, false);
            EXPECT_EQ(m.px, x);
            EXPECT_EQ(m.py, y);
            EXPECT_TRUE(m.in_range);
        }
    }
}

TEST(transform, scale_zooms_coordinates) {
    Mode7Params p;
    p.a = 0x0200;  // 2x horizontal zoom
    p.d = 0x0180;  // 1.5x vertical zoom
    const Mode7Raw r = mode7_transform(p, 10, 20);
    EXPECT_EQ(r.u, 20 * 256);
    EXPECT_EQ(r.v, 30 * 256);
    const MapPos m = to_map_pos(r, false);
    EXPECT_EQ(m.px, 20);
    EXPECT_EQ(m.py, 30);
}

TEST(transform, rotation_90_degrees) {
    Mode7Params p;
    p.a = 0;
    p.b = 0x0100;   // u = sy
    p.c = -0x0100;  // v = -sx
    p.d = 0;
    const Mode7Raw r = mode7_transform(p, 40, 15);
    EXPECT_EQ(r.u, 15 * 256);
    EXPECT_EQ(r.v, -40 * 256);
}

TEST(transform, center_offsets_shift_origin) {
    Mode7Params p;
    p.x0 = 100;
    p.y0 = 50;
    const Mode7Raw r = mode7_transform(p, 110, 60);  // sx=10, sy=10
    EXPECT_EQ(r.u, 10 * 256);
    EXPECT_EQ(r.v, 10 * 256);

    Mode7Params h;
    h.hofs = 3;
    h.vofs = 2;
    const Mode7Raw rh = mode7_transform(h, 4, 4);
    EXPECT_EQ(rh.u, 4 * 256 + 3 * 256);
    EXPECT_EQ(rh.v, 4 * 256 + 2 * 256);
}

TEST(transform, negative_floor_semantics) {
    Mode7Raw r;
    r.u = -1 * 256 - 128;  // -1.5 in 8.8
    r.v = 0;
    const MapPos m = to_map_pos(r, false);
    EXPECT_EQ(m.px, -2);  // floor(-1.5), not truncation
    EXPECT_EQ(m.tx, -1);
    EXPECT_EQ(m.ty, 0);
    EXPECT_FALSE(m.in_range);  // negative tiles are out of range unwrapped
}

namespace {
// One map cell -> distinct color per tile via CGRAM identity ramp.
struct Scene {
    Vram vram;
    Cgram cgram;
};
Scene make_scene() {
    Scene s;
    for (unsigned i = 1; i < 256; ++i) {
        s.cgram.e[i] = static_cast<uint16_t>(i * 257);
    }
    // Every tile is a solid block of its own number: tile t's pixel data
    // (64 bytes at t*64) all read t.
    for (unsigned t = 0; t < 256; ++t) {
        for (unsigned i = 0; i < 64; ++i) {
            vbyte(s.vram, t * 64u + i, static_cast<uint8_t>(t));
        }
    }
    return s;
}
}  // namespace

TEST(fetch, tile_byte_is_tile_number_and_pixel_index) {
    Scene s = make_scene();
    build_map(s.vram, 0x1800);  // word addr 0x1800 -> byte 0x3000
    // Tile (2,1): number (2 + 3) % 190 + 1 = 6.
    const MapPos pos{5, 9, 2, 1, true};  // px=5, py=9
    const uint8_t color = fetch_mode7_pixel(s.vram, 0x1800, pos);
    // Tile 6's data is solid, so the pixel value IS the tile number.
    EXPECT_EQ(color, 6u);
    // Out-of-range collapses to transparent.
    const MapPos bad{0, 0, 200, 0, false};
    EXPECT_EQ(fetch_mode7_pixel(s.vram, 0x1800, bad), 0u);
}

TEST(mode7frame, identity_passthrough_matches_map) {
    Scene s = make_scene();
    build_map(s.vram, 0x1800);
    Mode7Params p;
    p.wrap = true;
    std::array<uint8_t, kFbBytes> fb{};
    render_mode7_frame(p, s.vram, s.cgram, 0x1800, fb);
    // With wrap the whole screen is inside the map: pixel (x,y) shows
    // cgram.e[tile(x>>3, y>>3)] regardless of sub-tile position.
    for (int x = 0; x < 256; x += 37) {
        for (int y = 0; y < 224; y += 41) {
            const unsigned tx = static_cast<unsigned>(x) >> 3;
            const unsigned ty = static_cast<unsigned>(y) >> 3;
            const unsigned expect =
                ((tx + ty * 3u) % 190u + 1u) * 257u & 0xFFFFu;
            EXPECT_EQ(pixel_rgba(fb, x, y),
                      bgr555_to_rgba8(static_cast<uint16_t>(expect)));
        }
    }
}

TEST(mode7frame, out_of_range_backdrop_vs_wrap) {
    Scene s = make_scene();
    build_map(s.vram, 0x0000);
    s.cgram.e[0] = 0x001Fu;  // backdrop red

    // Zoom IN 8x: pixels past screen x=127 sample beyond map column 128.
    Mode7Params q;
    q.a = 0x0800;
    q.d = 0x0800;

    // Unwrapped: (200,111) -> map px 1600, out of range -> backdrop red.
    q.wrap = false;
    std::array<uint8_t, kFbBytes> fb{};
    render_mode7_frame(q, s.vram, s.cgram, 0, fb);
    EXPECT_EQ(pixel_rgba(fb, 200, 111), bgr555_to_rgba8(0x001F));
    // (40,40) -> map px 320, still on the map -> some non-backdrop color.
    EXPECT_NE(pixel_rgba(fb, 40, 40), bgr555_to_rgba8(0x001F));

    // Wrapped: the same coordinates fold back into real tiles, so the
    // backdrop disappears entirely from the frame.
    q.wrap = true;
    std::array<uint8_t, kFbBytes> fbw{};
    render_mode7_frame(q, s.vram, s.cgram, 0, fbw);
    EXPECT_NE(pixel_rgba(fbw, 200, 111), bgr555_to_rgba8(0x001F));
    bool differs = false;
    for (int i = 0; i < kScreenWidth && !differs; ++i) {
        differs = pixel_rgba(fb, i, 111) != pixel_rgba(fbw, i, 111);
    }
    EXPECT_TRUE(differs);
}

TEST(mode7frame, color_zero_is_backdrop) {
    Scene s = make_scene();
    // Park an all-zero map at word $1800 (byte $3000): every sample reads
    // tile 0, whose data is solid color 0 -> transparent -> backdrop.
    for (unsigned i = 0; i < 16384; ++i) {
        vbyte(s.vram, 0x3000u + i, 0u);
    }
    s.cgram.e[0] = 0x7C00u;  // blue backdrop
    Mode7Params p;
    std::array<uint8_t, kFbBytes> fb{};
    render_mode7_frame(p, s.vram, s.cgram, 0x1800, fb);
    EXPECT_EQ(pixel_rgba(fb, 0, 0), bgr555_to_rgba8(0x7C00));
    EXPECT_EQ(pixel_rgba(fb, 255, 223), bgr555_to_rgba8(0x7C00));
}
