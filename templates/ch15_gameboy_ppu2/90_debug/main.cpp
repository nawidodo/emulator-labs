// Tests for exercise 90: five seeded raster defects. Suites are named so
// each failing suite isolates its defect(s):
//   debug_modes   -> defect 1 (mode transition a line late)
//   debug_sprites -> defects 2+3 (x==0 hide inverted, priority flipped)
//   debug_lyc     -> defect 4 (LYC compared one line early)
//   debug_window  -> defect 5 (window content row from LY-WY)
#define LABSTEST_MAIN
#include <cstring>
#include <string>

#include "labstest.hpp"
#include "raster.hpp"

namespace {

using gbfix::PpuState;

PpuState baseState() {
    PpuState s{};
    s.lcdc = gbfix::kLcdcLcdOn | gbfix::kLcdcTileUnsigned |
             gbfix::kLcdcSpritesEnable | gbfix::kLcdcBgEnable;
    s.bgp = 0xE4;
    s.obp0 = 0xE4;
    s.obp1 = 0x1B;
    return s;
}

void putSolidTile(PpuState& s, uint8_t tile, uint8_t idx) {
    const uint8_t lo = idx & 1 ? 0xFF : 0x00;
    const uint8_t hi = idx & 2 ? 0xFF : 0x00;
    for (int r = 0; r < 8; ++r) {
        s.vram[tile * 16 + 2 * r] = lo;
        s.vram[tile * 16 + 2 * r + 1] = hi;
    }
}

void fillMap(PpuState& s, uint16_t mapBase, uint8_t entry) {
    std::memset(&s.vram[mapBase], entry, 0x800);
}

void setOam(PpuState& s, int entry, uint8_t y, uint8_t x, uint8_t tile,
            uint8_t flags) {
    s.oam[4 * entry + 0] = y;
    s.oam[4 * entry + 1] = x;
    s.oam[4 * entry + 2] = tile;
    s.oam[4 * entry + 3] = flags;
}

uint8_t shadeAt(const uint8_t rgba[160][4], int x) {
    for (int sh = 0; sh < 4; ++sh)
        if (rgba[x][0] == gbfix::kGray[sh]) return static_cast<uint8_t>(sh);
    return 99;
}

}  // namespace

// ---- defect 1 -----------------------------------------------------------

TEST(debug_modes, hblank_starts_at_dot_252_of_every_visible_line) {
    EXPECT_TRUE(gbfix::modeAt(5, 251) == 3);
    EXPECT_TRUE(gbfix::modeAt(5, 252) == 0);
    EXPECT_TRUE(gbfix::modeAt(100, 455) == 0);
    EXPECT_TRUE(gbfix::modeAt(100, 80) == 3);
}

TEST(debug_modes, trace_shows_unlock_on_each_line) {
    const std::string trace = gbfix::buildModeTrace(gbfix::kFrameDots);
    // The VRAM/OAM unlock edge must appear on EVERY visible line.
    for (int ly = 0; ly < 144; ly += 17) {
        const std::string needle = "ly=" + std::to_string(ly) +
                                   " dot=252 mode=0\n";
        EXPECT_NE(trace.find(needle), std::string::npos);
    }
    EXPECT_NE(trace.find("ly=0 dot=80 mode=3\n"), std::string::npos);
    EXPECT_NE(trace.find("ly=144 dot=0 mode=1\n"), std::string::npos);
}


// ---- defects 2 and 3 ----------------------------------------------------

TEST(debug_sprites, normal_sprite_drawn) {
    PpuState s = baseState();
    putSolidTile(s, 1, 0);  // BG: white
    putSolidTile(s, 2, 3);  // sprite tile: dark
    fillMap(s, 0x1800, 1);
    setOam(s, 0, 70, 40, 2, 0);
    gbfix::Sprite list[10] = {};
    EXPECT_EQ(gbfix::collectSpritesForLine(s, 60, list), 1);
    EXPECT_EQ(list[0].x, 40);

    // And the composed line shows it via the glue path.
    s.scx = 0;
    s.scy = 0;
    PpuState t = s;
    gbfix::Frame frame{};
    bool winOn[144] = {};
    gbfix::renderFrameWindow(t, frame, winOn);
    EXPECT_EQ(shadeAt(frame[60], 36), 3);
    EXPECT_EQ(shadeAt(frame[60], 20), 0);
}

TEST(debug_sprites, x_zero_entry_is_hidden) {
    PpuState s = baseState();
    putSolidTile(s, 1, 0);
    putSolidTile(s, 2, 3);
    fillMap(s, 0x1800, 1);
    setOam(s, 0, 70, 0, 2, 0);  // hidden by hardware
    gbfix::Sprite list[10] = {};
    EXPECT_EQ(gbfix::collectSpritesForLine(s, 60, list), 0);

    gbfix::Frame frame{};
    bool winOn[144] = {};
    gbfix::renderFrameWindow(s, frame, winOn);
    EXPECT_EQ(shadeAt(frame[60], 0), 0);   // nothing drawn at column 0
    EXPECT_EQ(shadeAt(frame[60], 159), 0);
}

TEST(debug_sprites, priority_flag_hides_only_on_nonzero_bg_index) {
    // Flag SET over nonzero BG index => sprite hidden.
    EXPECT_FALSE(gbfix::spritePixelWins(gbfix::kFlagBgPriority, 3, 1));
    // Flag SET over BG index 0 => sprite shows.
    EXPECT_TRUE(gbfix::spritePixelWins(gbfix::kFlagBgPriority, 3, 0));
    // No flag => sprite always shows (nonzero index).
    EXPECT_TRUE(gbfix::spritePixelWins(0, 3, 1));
    EXPECT_TRUE(gbfix::spritePixelWins(0, 3, 0));
    // Tile index 0 is transparent regardless of anything.
    EXPECT_FALSE(gbfix::spritePixelWins(0, 0, 0));
    EXPECT_FALSE(gbfix::spritePixelWins(gbfix::kFlagBgPriority, 0, 1));
}

// ---- defect 4 -----------------------------------------------------------

TEST(debug_lyc, coincidence_is_exact_equality) {
    EXPECT_TRUE(gbfix::coincidenceFlag(64, 64));
    EXPECT_TRUE(gbfix::coincidenceFlag(0, 0));
    EXPECT_FALSE(gbfix::coincidenceFlag(63, 64));
    EXPECT_FALSE(gbfix::coincidenceFlag(65, 64));
}

TEST(debug_lyc, scripted_irq_log_fires_at_lyc) {
    // Walk one frame sampling at each line start with only the LYC source
    // enabled; the interrupt must fire EXACTLY at LYC = 64.
    struct Edge { bool prev = false, line = false; };
    auto feed = [](Edge& d, bool sig) {
        d.prev = d.line;
        d.line = sig;
        return d.line && !d.prev;
    };
    Edge d;
    int fires = 0;
    int fireLine = -1;
    for (int ly = 0; ly < 154; ++ly) {
        const bool sig = gbfix::coincidenceFlag(ly, 64);
        if (feed(d, sig)) { ++fires; fireLine = ly; }
    }
    EXPECT_EQ(fires, 1);
    EXPECT_EQ(fireLine, 64);
}


TEST(debug_window, full_run_content_advances_one_per_line) {
    PpuState s = baseState();
    s.lcdc |= gbfix::kLcdcWinEnable | gbfix::kLcdcWinMapHi;
    putSolidTile(s, 1, 0);
    putSolidTile(s, 2, 3);
    putSolidTile(s, 3, 1);
    fillMap(s, 0x1800, 1);
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 32; ++c) s.vram[0x1C00 + r * 32 + c] = 2;
    for (int r = 3; r < 32; ++r)
        for (int c = 0; c < 32; ++c) s.vram[0x1C00 + r * 32 + c] = 3;

    s.wy = 0;
    s.wx = 7;

    // Without any toggle, content row equals LY: row 24/8=3 is already
    // light. Probe inside row 2 vs row 3:
    PpuState plain = s;
    gbfix::Frame f1{};
    bool all[144];
    for (bool& b : all) b = true;
    gbfix::renderFrameWindow(plain, f1, all);
    EXPECT_EQ(shadeAt(f1[23], 80), 3);  // content 23 -> tile row 2 (dark)
    EXPECT_EQ(shadeAt(f1[24], 80), 1);  // content 24 -> tile row 3 (light)
}

TEST(debug_window, midframe_disable_skips_rows_does_not_restart) {
    PpuState s = baseState();
    s.lcdc |= gbfix::kLcdcWinEnable | gbfix::kLcdcWinMapHi;
    putSolidTile(s, 1, 0);
    putSolidTile(s, 2, 3);
    putSolidTile(s, 3, 1);
    fillMap(s, 0x1800, 1);
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 32; ++c) s.vram[0x1C00 + r * 32 + c] = 2;
    for (int r = 3; r < 32; ++r)
        for (int c = 0; c < 32; ++c) s.vram[0x1C00 + r * 32 + c] = 3;


    s.wy = 0;
    s.wx = 7;
    gbfix::Frame frame{};
    bool win[144];
    for (bool& b : win) b = true;
    for (int ly = 40; ly < 48; ++ly) win[ly] = false;
    gbfix::renderFrameWindow(s, frame, win);

    // Line 48 draws content row 40 (tile row 5, LIGHT). A restart-from-WY
    // implementation would draw content row 8 (tile row 1, DARK).
    EXPECT_EQ(shadeAt(frame[48], 80), 1);
    // Line 88 -> content row 80 -> tile row 10 (light).
    EXPECT_EQ(shadeAt(frame[88], 80), 1);
}
