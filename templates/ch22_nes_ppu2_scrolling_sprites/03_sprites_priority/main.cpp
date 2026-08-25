#define LABSTEST_MAIN
#include "labstest.hpp"

#include <array>
#include <cstdint>

#include "fixture.hpp"
#include "render22.hpp"

using nes22prio::Mirroring;
using nes22prio::Scene;
using nes22fix::Snapshot;

namespace {

Snapshot base_scene() {
    Snapshot s;
    s.mirroring = 1;   // vertical: address bit 10 selects the physical page
    s.mask = 0x1E;     // bg + sprites on, no left-column clips
    return s;
}

Scene to_scene(const Snapshot& s) {
    Scene sc;
    sc.chr = s.chr.data();
    sc.nt = s.nt.data();
    sc.pal = s.pal.data();
    sc.oam = s.oam.data();
    sc.mirroring = Mirroring::Vertical;
    sc.ctrl = s.ctrl;
    sc.mask = s.mask;
    return sc;
}

void solid_tile(Snapshot& s, int idx, int color) {
    uint8_t lo = (color & 1) ? 0xFF : uint8_t(0);
    uint8_t hi = (color & 2) ? 0xFF : uint8_t(0);
    for (int y = 0; y < 8; ++y) {
        s.chr[idx * 16 + y] = lo;
        s.chr[idx * 16 + 8 + y] = hi;
    }
}

constexpr size_t kPx = 256u * 240u * 4;

}  // namespace

TEST(nes22render, fine_x_shifts_background_by_pixels) {
    Snapshot s = base_scene();
    solid_tile(s, 1, 1);
    solid_tile(s, 2, 2);
    for (int r = 0; r < 30; ++r)
        for (int c = 0; c < 32; ++c) s.nt[r * 32 + c] = uint8_t((c % 2) ? 2 : 1);
    s.pal[0x01] = 0x11;
    s.pal[0x02] = 0x16;

    Scene sc = to_scene(s);
    sc.l.t = 0;

    sc.l.x = 0;
    std::array<uint8_t, kPx> f0{};
    nes22prio::render_frame(f0, sc);
    EXPECT_EQ(f0[(120u * 256u + 4u) * 4u], 8);       // tile 1 -> $11

    sc.l.x = 4;                                      // fine X scroll
    std::array<uint8_t, kPx> f1{};
    nes22prio::render_frame(f1, sc);
    // Source column for screen x=2 is now 6: still tile 1.
    EXPECT_EQ(f1[(120u * 256u + 2u) * 4u + 2u], 196);
    // Source column for screen x=6 is 10: tile 2.
    EXPECT_EQ(f1[(120u * 256u + 6u) * 4u + 0u], 152);  // $16 R=152
}

TEST(nes22render, vertical_scroll_walks_nametables_with_flip) {
    Snapshot s = base_scene();
    solid_tile(s, 1, 1);
    solid_tile(s, 2, 2);
    for (int c = 0; c < 32; ++c) {
        s.nt[c] = 1;            // physical page A: tile 1
        s.nt[0x400 + c] = 2;    // physical page B: tile 2
    }
    for (int r = 1; r < 30; ++r)
        for (int c = 0; c < 32; ++c) {
            s.nt[r * 32 + c] = 1;
            s.nt[0x400 + r * 32 + c] = 2;
        }
    s.pal[0x01] = 0x11;
    s.pal[0x02] = 0x16;
    Scene sc = to_scene(s);
    sc.mirroring = Mirroring::Horizontal;  // bit 11 selects the page
    sc.l.t = uint16_t(8 << 5);   // coarse Y = 8, nametable A

    std::array<uint8_t, kPx> frame{};
    nes22prio::render_frame(frame, sc);
    // Each coarse row spans 8 lines (fine Y). Starting at coarse Y 8 on
    // line 0, coarse 29 ends at line 175; the wrap flips to page B and
    // every line from 176 to 239 renders from physical page B.
    EXPECT_EQ(frame[(0u * 256u + 4u) * 4u], 8);      // $11 from page A
    EXPECT_EQ(frame[(175u * 256u + 4u) * 4u], 8);    // still page A
    EXPECT_EQ(frame[(176u * 256u + 4u) * 4u], 152);  // $16: flipped to B
    EXPECT_EQ(frame[(239u * 256u + 4u) * 4u], 152);  // page B to the end
}

TEST(nes22render, sprite_priority_first_wins_and_behind_bg) {
    Snapshot s = base_scene();
    auto put = [&](int i, uint8_t y, uint8_t t, uint8_t a, uint8_t x) {
        s.oam[i * 4] = y; s.oam[i * 4 + 1] = t;
        s.oam[i * 4 + 2] = a; s.oam[i * 4 + 3] = x;
    };
    put(0, 50, 1, 0x00, 100);   // palette 0, in front
    put(1, 50, 2, 0x02, 100);   // palette 2, same spot
    solid_tile(s, 1, 1);
    solid_tile(s, 2, 2);
    s.pal[0x11] = 0x21;         // sprite pal 1, color 1 -> blue-ish
    s.pal[0x1A] = 0x16;         // sprite pal 2, color 2 -> red-ish

    Scene sc = to_scene(s);
    sc.l.t = 0;
    std::array<uint8_t, kPx> frame{};
    nes22prio::render_frame(frame, sc);
    EXPECT_EQ(frame[(52u * 256u + 104u) * 4u], 76);  // $21: sprite 0 wins

    // Behind-bg bit on sprite 0 + opaque background: bg shows instead.
    put(0, 50, 1, 0x20, 100);   // behind-bg bit set
    solid_tile(s, 3, 3);
    for (int i = 0; i < 0x400; ++i) s.nt[i] = 3;
    s.pal[0x03] = 0x28;
    std::array<uint8_t, kPx> frame2{};
    nes22prio::render_frame(frame2, sc);
    EXPECT_EQ(frame2[(52u * 256u + 104u) * 4u + 1u], 170);  // bg $28
}

TEST(nes22render, sprite0_hit_reported_and_clipped) {
    Snapshot s = base_scene();
    solid_tile(s, 3, 1);
    for (int i = 0; i < 0x400; ++i) s.nt[i] = 3;  // opaque bg everywhere
    solid_tile(s, 5, 2);
    s.oam[0] = 60; s.oam[1] = 5; s.oam[3] = 100; // sprite 0 on-screen
    s.pal[0x05] = 0x12;
    s.pal[0x12] = 0x16;

    Scene sc = to_scene(s);
    sc.l.t = 0;
    std::array<uint8_t, kPx> frame{};
    EXPECT_EQ(nes22prio::render_frame(frame, sc), true);

    // Park sprite 0 entirely inside the clipped left column: no hit.
    s.oam[3] = 0;
    sc.mask = uint8_t(s.mask & ~0x04);           // PPUMASK bit2 clear
    std::array<uint8_t, kPx> frame2{};
    EXPECT_EQ(nes22prio::render_frame(frame2, sc), false);
}
