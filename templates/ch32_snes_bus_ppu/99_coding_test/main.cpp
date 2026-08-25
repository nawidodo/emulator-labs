// Coding test entry point. Custom main(): parses [--filter F],
// --snapshot PATH and --hash-frame FILE, runs the labstest suite, then
// (when a snapshot is given) parses and renders the snapshot file through
// the student's implementation and cross-checks it against an independent
// reference oracle.
//
// Exit code is nonzero on any test failure or snapshot check failure, so
// hidden manifests can gate on expect_exit plus the frame hash.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstddef>

#include "labstest.hpp"
#include "coding.hpp"

using namespace snesbus;

namespace {

// Independent reference model written straight from SPEC.md. Deliberately
// does NOT call the student's parse_map_row/decode_tile_row/
// snapshot_sample/snapshot_render.
struct Reference {
    unsigned w = 0;
    unsigned h = 0;
    std::vector<unsigned> map;
    std::vector<unsigned> tiles;   // tile*64 + r*8 + c
    std::array<uint16_t, 256> pal{};
    unsigned scroll_x = 0;
    unsigned scroll_y = 0;
    int win_l = 0;
    int win_r = 255;
    bool win_en = false;
    bool win_inv = false;
    bool sub = false;
    bool half = false;
    bool math = false;
    unsigned backdrop = 0;

    explicit Reference(const Snapshot& s) {
        w = s.map_w;
        h = s.map_h;
        map.assign(s.map.begin(), s.map.end());
        for (unsigned t = 0; t < 256; ++t) {
            for (unsigned i = 0; i < 64; ++i) {
                tiles.push_back(s.tiles[t][i]);
            }
        }
        pal = s.pal;
        scroll_x = s.scroll_x;
        scroll_y = s.scroll_y;
        win_l = static_cast<int>(s.win_left);
        win_r = static_cast<int>(s.win_right);
        win_en = s.win_enable;
        win_inv = s.win_invert;
        sub = s.math_sub;
        half = s.math_half;
        math = s.math_on;
        backdrop = s.backdrop;
    }

    // Texel values select CGRAM entries directly in this mini format.
    int expected_entry(int x, int y) const {
        const unsigned mw = w * 8u;
        const unsigned mh = h * 8u;
        const int sx =
            ((x + static_cast<int>(scroll_x)) % static_cast<int>(mw) +
             static_cast<int>(mw)) %
            static_cast<int>(mw);
        const int sy =
            ((y + static_cast<int>(scroll_y)) % static_cast<int>(mh) +
             static_cast<int>(mh)) %
            static_cast<int>(mh);
        const unsigned tile =
            map[static_cast<size_t>(sy / 8 * static_cast<int>(w) + sx / 8)];
        return static_cast<int>(
            tiles[static_cast<size_t>(tile) * 64u +
                  static_cast<unsigned>(sy % 8) * 8u +
                  static_cast<unsigned>(sx % 8)]);
    }
    uint32_t expected_rgba(int x, int y) const {
        bool visible = true;
        if (win_en) {
            const bool inside = x >= win_l && x <= win_r;
            visible = inside != win_inv;
        }
        unsigned idx = backdrop;
        if (visible) {
            const unsigned v = static_cast<unsigned>(expected_entry(x, y));
            if (v != 0) {
                idx = v;  // texel selects CGRAM directly; 0 = transparent
            }
        }
        auto expand = [](unsigned v) { return (v << 3) | (v >> 2); };
        auto chan = [&](uint16_t c, unsigned sh) {
            return static_cast<int>((c >> sh) & 31u);
        };
        int rgb[3];
        for (int i = 0; i < 3; ++i) {
            const unsigned shifts[3] = {0, 5, 10};
            int d;
            if (math && idx != backdrop) {
                d = sub ? chan(pal[idx], shifts[i]) -
                               chan(pal[backdrop], shifts[i])
                        : chan(pal[idx], shifts[i]) +
                               chan(pal[backdrop], shifts[i]);
                if (half) {
                    d /= 2;
                }
                d = d < 0 ? 0 : (d > 31 ? 31 : d);
            } else {
                d = chan(pal[idx], shifts[i]);
            }
            rgb[i] = expand(static_cast<unsigned>(d));
        }
        // rgb = {r,g,b} expanded; pack as RGBA bytes b,g,r,a to match the
        // renderer's storage order.
        return 0xFF000000u | static_cast<unsigned>(rgb[2]) |
               (static_cast<unsigned>(rgb[1]) << 8) |
               (static_cast<unsigned>(rgb[0]) << 16);
    }
};


std::string slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

int run_snapshot_checks(const std::string& path, const std::string& hash_out,
                        bool* ok) {
    const std::string text = slurp(path);
    if (text.empty()) {
        std::fprintf(stderr, "cannot read snapshot %s\n", path.c_str());
        *ok = false;
        return 1;
    }
    Snapshot s;
    try {
        s = parse_snapshot(text);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "parse failed: %s\n", e.what());
        *ok = false;
        return 1;
    }

    // Render twice: must be byte-identical (determinism contract).
    std::vector<uint8_t> fb1(kSnapWidth * kSnapHeight * 4);
    std::vector<uint8_t> fb2(kSnapWidth * kSnapHeight * 4);
    snapshot_render(s, fb1);
    snapshot_render(s, fb2);
    if (fb1 != fb2) {
        std::fprintf(stderr, "nondeterministic render\n");
        *ok = false;
    }

    // Cross-check a deterministic sample grid against the oracle.
    const Reference ref(s);
    int bad = 0;
    for (int y = 0; y < kSnapHeight && bad < 5; y += 13) {
        for (int x = 0; x < kSnapWidth && bad < 5; x += 11) {
            const size_t o =
                (static_cast<size_t>(y) * kSnapWidth + x) * 4u;
            const uint32_t got = fb1[o] | (fb1[o + 1] << 8) |
                                 (fb1[o + 2] << 16) |
                                 (static_cast<uint32_t>(fb1[o + 3]) << 24);
            if (got != ref.expected_rgba(x, y)) {
                std::fprintf(stderr,
                             "oracle mismatch at (%d,%d): got %08X want %08X\n",
                             x, y, got, ref.expected_rgba(x, y));
                ++bad;
            }
        }
    }
    if (bad != 0) {
        *ok = false;
    }

    if (!hash_out.empty()) {
        std::ofstream out(hash_out, std::ios::binary);
        if (!out) {
            std::fprintf(stderr, "cannot write %s\n", hash_out.c_str());
            *ok = false;
            return 1;
        }
        out.write(reinterpret_cast<const char*>(fb1.data()),
                  static_cast<std::streamsize>(fb1.size()));
        std::printf("wrote %s (%zu bytes)\n", hash_out.c_str(), fb1.size());
    }
    if (*ok) {
        std::printf("snapshot ok: %s (%ux%u map)\n", path.c_str(), s.map_w,
                    s.map_h);
    }
    return *ok ? 0 : 1;
}

}  // namespace

TEST(spec, example_parse_and_tiles) {
    // The SPEC.md example (kept byte-identical with the doc).
    const std::string snap = R"(# SPEC example
SIZE 4 2
PAL 0 3800
PAL 1 03E0
PAL 2 7C00
PAL 3 7FFF
TILE 0 0000 0000 0000 0000 0000 0000 0000 0000
TILE 1 5555 5555 5555 5555 5555 5555 5555 5555
TILE 2 AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA
TILE 3 1B1B 1B1B 1B1B 1B1B 1B1B 1B1B 1B1B 1B1B
MAP 0 1 0 2 3
MAP 1 3 3 1 0
SCROLL 0 0
WINDOW 16 47 1 0
MATH add 1 1
BACKDROP 0
)";
    const Snapshot s = parse_snapshot(snap);
    EXPECT_EQ(s.map_w, 4u);
    EXPECT_EQ(s.map_h, 2u);
    EXPECT_EQ(s.map[0], 1u);
    EXPECT_EQ(s.map[3], 3u);
    EXPECT_EQ(s.map[4], 3u);
    EXPECT_EQ(s.pal[1], 0x03E0);

    // Tile 1 rows are solid value 1; pixel k of row word $5555 is 1.
    std::array<uint8_t, 8> px{};
    decode_tile_row(0x5555, &px);
    EXPECT_EQ(px[0], 1u);
    EXPECT_EQ(px[7], 1u);
    decode_tile_row(0xAAAA, &px);
    EXPECT_EQ(px[0], 2u);
    EXPECT_EQ(px[3], 2u);
    // Checker word $1B1B: pixels 0..7 = 0,1,2,3,0,1,2,3.
    decode_tile_row(0x1B1B, &px);
    for (unsigned k = 0; k < 8; ++k) {
        EXPECT_EQ(px[k], k % 4u);
    }
}

TEST(spec, example_render_pixels) {
    const std::string snap = R"(SIZE 4 2
PAL 0 3800
PAL 1 03E0
PAL 2 7C00
PAL 3 7FFF
TILE 0 0000 0000 0000 0000 0000 0000 0000 0000
TILE 1 5555 5555 5555 5555 5555 5555 5555 5555
TILE 2 AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA
TILE 3 1B1B 1B1B 1B1B 1B1B 1B1B 1B1B 1B1B 1B1B
MAP 0 1 0 2 3
MAP 1 3 3 1 0
SCROLL 0 0
WINDOW 16 47 1 0
MATH add 1 1
BACKDROP 0
)";
    const Snapshot s = parse_snapshot(snap);
    std::vector<uint8_t> fb(kSnapWidth * kSnapHeight * 4);
    snapshot_render(s, fb);
    const auto px = [&](int x, int y) {
        const size_t o =
            (static_cast<size_t>(y) * kSnapWidth + x) * 4u;
        return fb[o] | (fb[o + 1] << 8) | (fb[o + 2] << 16) |
               (static_cast<uint32_t>(fb[o + 3]) << 24);
    };
    // Window [16,47]: inside it the layer shows; outside -> backdrop.
    // Inside at (20,4): map covers x<32, tile (2,0)=2 solid blue; MATH
    // add-half vs backdrop 0x3800 (blue channel 14): fg blue=31 ->
    // (r,g,b)=(0,0,(31+14)/2=22) -> BGR555 (22<<10).
    EXPECT_EQ(px(20, 4),
              snap_bgr555_to_rgba8(static_cast<uint16_t>(22u << 10)));
    // Outside window at (100,4): pure backdrop 0x3800.
    EXPECT_EQ(px(100, 4), snap_bgr555_to_rgba8(0x3800));
}

TEST(spec, reject_malformed_snapshots) {
    bool threw = false;
    try {
        parse_snapshot("SIZE 4 2\nMAP 0 1 2 3\n");  // wrong token count
    } catch (const std::exception&) {
        threw = true;
    }
    EXPECT_TRUE(threw);
    threw = false;
    try {
        parse_snapshot("FROB 7\n");
    } catch (const std::exception&) {
        threw = true;
    }
    EXPECT_TRUE(threw);
}

int main(int argc, char** argv) {
    std::string filter;
    std::string snapshot;
    std::string hash_out;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&](std::string* out) {
            if (i + 1 >= argc) {
                return false;
            }
            *out = argv[++i];
            return true;
        };
        if (a == "--filter") {
            if (!next(&filter)) {
                return 2;
            }
        } else if (a == "--snapshot") {
            if (!next(&snapshot)) {
                return 2;
            }
        } else if (a == "--hash-frame") {
            if (!next(&hash_out)) {
                return 2;
            }
        } else if (a == "--help" || a == "-h") {
            std::printf(
                "usage: %s [--filter F] [--snapshot PATH] "
                "[--hash-frame FILE]\n",
                argv[0]);
            return 0;
        } else {
            std::fprintf(stderr, "unknown flag: %s\n", a.c_str());
            return 2;
        }
    }

    labstest::run_all(filter);
    bool ok = labstest::failures() == 0;
    int snap_rc = 0;
    if (!snapshot.empty()) {
        snap_rc = run_snapshot_checks(snapshot, hash_out, &ok);
    }
    return (ok && snap_rc == 0) ? 0 : 1;
}
