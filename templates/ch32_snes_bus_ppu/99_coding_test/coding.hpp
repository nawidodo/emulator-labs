#pragma once
// Coding test — unseen-spec TEXT snapshot renderer.
//
// CODING_TEST.md hands you SPEC.md (the snapshot grammar below) and an
// API skeleton; you implement the four @LABS blocks so ANY conforming
// snapshot parses and renders. The public suite pins the SPEC example;
// hidden grading renders an UNSEEN snapshot file through your code.
//
// Snapshot grammar (line-based, '#' comments, blank lines ignored):
//
//   SIZE <w> <h>              tilemap size in tiles, 1..32 each
//   MAP <y> <t...>            row y: exactly w hex tile numbers (0..255)
//   PAL <i> <hhhh>            CGRAM entry i (0..255) = 4-hex BGR555
//   TILE <n> <hhhh x8>        tile n (0..255): eight 4-hex-digit rows;
//                             pixel k (0 = leftmost) = bits (14 - 2k),
//                             (15 - 2k) of the row word — 2bpp, MSB first
//   SCROLL <x> <y>            pixel scroll offsets (wrap over the map)
//   WINDOW <l> <r> <en> <inv> inclusive rect; en/inv are 0 or 1
//   MATH <add|sub> <half> <on>  e.g. "MATH add 1 1"
//   BACKDROP <i>              CGRAM index behind everything (default 0)
//
// Rendering model (deliberately simpler than exercises 03/04):
//   * One 2bpp layer. Tile pixel values select CGRAM entries DIRECTLY
//     (value 0 is transparent -> the backdrop entry shows).
//   * The map's top-left corner sits at screen (0,0); SCROLL wraps within
//     the map area (w*8 x h*8 pixels). Screen positions outside the map
//     area show the backdrop.
//   * Window semantics match exercise 03: when enabled, a pixel shows the
//     layer ONLY inside the effective window ([l,r] inclusive; invert
//     swaps inside/outside). Outside -> backdrop.
//   * When MATH is on, the surviving color combines with the backdrop
//     entry (add/sub, optional halving, saturating clamp in 5-bit
//     channels) exactly like apply_color_math() in exercise 03.

#include <array>
#include <cstdint>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace snesbus {

inline constexpr int kSnapWidth = 256;
inline constexpr int kSnapHeight = 224;

inline constexpr uint16_t kBackdropSentinel = 0xFFFFu;

struct Snapshot {
    unsigned map_w = 0;
    unsigned map_h = 0;
    std::vector<uint16_t> map{};                 // map_h rows x map_w tiles
    std::array<uint16_t, 256> pal{};             // BGR555 entries
    std::array<std::array<uint8_t, 64>, 256> tiles{};  // tile -> 8x8 pixels
    unsigned scroll_x = 0;
    unsigned scroll_y = 0;
    unsigned win_left = 0;
    unsigned win_right = 255;
    bool win_enable = false;
    bool win_invert = false;
    bool math_sub = false;
    bool math_half = false;
    bool math_on = false;
    unsigned backdrop = 0;
};

inline uint32_t snap_bgr555_to_rgba8(uint16_t c) {
    const auto expand = [](unsigned v) {
        return static_cast<uint32_t>((v << 3) | (v >> 2));
    };
    return 0xFF000000u | expand((c >> 10) & 0x1Fu) |
           (expand((c >> 5) & 0x1Fu) << 8) | (expand(c & 0x1Fu) << 16);
}

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Store one MAP row: `tokens` holds exactly map_w hex tile numbers for
// row y. Returns false on any out-of-range tile or wrong token count.
inline bool parse_map_row(Snapshot& s, unsigned y,
                          const std::vector<std::string>& tokens) {
    if (tokens.size() != s.map_w || y >= s.map_h) {
        return false;
    }
    for (unsigned x = 0; x < s.map_w; ++x) {
        const unsigned t =
            static_cast<unsigned>(std::stoul(tokens[x], nullptr, 16));
        if (t > 0xFFu) {
            return false;
        }
        s.map[y * s.map_w + x] = static_cast<uint16_t>(t);
    }
    return true;
}
//@LABS-STUB
// TODO(1): validate and store one MAP row of hex tile numbers into s.map
// (row-major, y * map_w + x). Reject wrong token counts, bad rows and
// tile numbers above 255 by returning false.
inline bool parse_map_row(Snapshot&, unsigned,
                          const std::vector<std::string>&) {
    return true;  // wrong on purpose: accepts anything, stores nothing
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Decode one tile row word into 8 pixel values (0..3). Pixel k (0 =
// leftmost) lives in bits 15-14 for k=0, 13-12 for k=1, ... i.e.
// (word >> (14 - 2*k)) & 3.
inline void decode_tile_row(uint16_t row_word, std::array<uint8_t, 8>* out) {
    for (unsigned k = 0; k < 8; ++k) {
        (*out)[k] = static_cast<uint8_t>((row_word >> (14 - 2 * k)) & 3u);
    }
}
//@LABS-STUB
// TODO(2): shift each 2-bit pixel out of the row word, leftmost pixel
// first: pixel k = (row_word >> (14 - 2*k)) & 3.
inline void decode_tile_row(uint16_t, std::array<uint8_t, 8>*) {
    // TODO(2): replace this body.
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// Sample the snapshot layer at screen (x, y) AFTER scrolling: returns the
// selected CGRAM entry index, kBackdropSentinel when the point falls
// outside the map area (which always shows the backdrop), or 0 for a
// transparent texel (also rendered as backdrop by the renderer).
inline uint16_t snapshot_sample(const Snapshot& s, int x, int y) {
    const unsigned mw = s.map_w * 8u;
    const unsigned mh = s.map_h * 8u;
    if (mw == 0 || mh == 0) {
        return kBackdropSentinel;
    }
    const int sx = (x + static_cast<int>(s.scroll_x)) % static_cast<int>(mw);
    const int sy = (y + static_cast<int>(s.scroll_y)) % static_cast<int>(mh);
    const unsigned ux = static_cast<unsigned>(sx);
    const unsigned uy = static_cast<unsigned>(sy);
    const uint16_t tile = s.map[(uy / 8u) * s.map_w + ux / 8u];
    return s.tiles[tile][(uy % 8u) * 8u + ux % 8u];
}
//@LABS-STUB
// TODO(3): scroll-wrap (x,y) into the map area (mod map_w*8 / map_h*8),
// fetch the tile from s.map and index its 8x8 pixels. Return
// kBackdropSentinel when the map is empty.
inline uint16_t snapshot_sample(const Snapshot&, int, int) {
    return kBackdropSentinel;  // wrong on purpose: all backdrop
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// Render a full 256x224 frame (RGBA8888) from the snapshot:
// sample -> window gate -> transparency -> optional color math vs the
// backdrop entry -> expand to RGBA. Window rule: when enabled, the layer
// shows only inside the effective window (invert swaps inside/outside).
inline void snapshot_render(const Snapshot& s, std::span<uint8_t> out) {
    const auto clamp5 = [](int v) {
        return v < 0 ? 0 : (v > 31 ? 31 : v);
    };
    for (int y = 0; y < kSnapHeight; ++y) {
        for (int x = 0; x < kSnapWidth; ++x) {
            bool visible = true;
            if (s.win_enable) {
                const bool inside =
                    x >= static_cast<int>(s.win_left) &&
                    x <= static_cast<int>(s.win_right);
                visible = (inside != s.win_invert);
            }
            uint16_t idx = s.backdrop;
            if (visible) {
                const uint16_t v = snapshot_sample(s, x, y);
                if (v != kBackdropSentinel && v != 0) {
                    idx = v;  // texel value selects the CGRAM entry directly
                }
            }
            uint16_t color = s.pal[idx];
            if (s.math_on && idx != s.backdrop) {
                const auto chan = [&](uint16_t c, unsigned shift) {
                    return static_cast<int>((c >> shift) & 0x1Fu);
                };
                const unsigned shifts[3] = {0, 5, 10};
                int out_ch[3];
                for (int i = 0; i < 3; ++i) {
                    const int a = chan(color, shifts[i]);
                    const int b = chan(s.pal[s.backdrop], shifts[i]);
                    int d = s.math_sub ? a - b : a + b;
                    if (s.math_half) {
                        d /= 2;
                    }
                    out_ch[i] = clamp5(d);
                }
                color = static_cast<uint16_t>(out_ch[0] | (out_ch[1] << 5) |
                                              (out_ch[2] << 10));
            }
            const uint32_t rgba = snap_bgr555_to_rgba8(color);
            const size_t o = (static_cast<size_t>(y) * kSnapWidth + x) * 4u;
            out[o] = static_cast<uint8_t>(rgba);
            out[o + 1] = static_cast<uint8_t>(rgba >> 8);
            out[o + 2] = static_cast<uint8_t>(rgba >> 16);
            out[o + 3] = static_cast<uint8_t>(rgba >> 24);
        }
    }
}
//@LABS-STUB
// TODO(4): loop every pixel: window gate, sample, transparency fallback to
// s.pal[s.backdrop], optional MATH vs the backdrop entry (add/sub, half,
// clamp 0..31), expand with snap_bgr555_to_rgba8 and store RGBA.
inline void snapshot_render(const Snapshot&, std::span<uint8_t> out) {
    // Wrong on purpose: flat black frame.
    for (size_t i = 0; i < out.size(); i += 4) {
        out[i] = out[i + 1] = out[i + 2] = 0;
        out[i + 3] = 0xFF;
    }
}
//@LABS-END

// ---------------------------------------------------------------------------
// Parser framework (provided): dispatches lines of the grammar to the
// hooks above. Students do not modify this part.
// ---------------------------------------------------------------------------

[[noreturn]] inline void snap_error(const std::string& msg) {
    throw std::runtime_error("snapshot: " + msg);
}

inline Snapshot parse_snapshot(const std::string& text) {
    Snapshot s;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        const size_t hash = line.find('#');
        if (hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        std::istringstream ls(line);
        std::string key;
        if (!(ls >> key)) {
            continue;
        }
        std::vector<std::string> tok;
        std::string t;
        while (ls >> t) {
            tok.push_back(t);
        }
        if (key == "SIZE") {
            if (tok.size() != 2) snap_error("SIZE needs 2 args");
            s.map_w = std::stoul(tok[0]);
            s.map_h = std::stoul(tok[1]);
            if (s.map_w == 0 || s.map_w > 32 || s.map_h == 0 ||
                s.map_h > 32) {
                snap_error("SIZE out of range");
            }
            s.map.assign(s.map_w * s.map_h, 0);
        } else if (key == "MAP") {
            if (tok.empty()) snap_error("MAP needs a row index");
            if (!parse_map_row(s, std::stoul(tok[0]),
                               std::vector<std::string>(tok.begin() + 1,
                                                        tok.end()))) {
                snap_error("bad MAP row");
            }
        } else if (key == "PAL") {
            if (tok.size() != 2) snap_error("PAL needs 2 args");
            const unsigned i = std::stoul(tok[0]);
            if (i > 255) snap_error("PAL index out of range");
            s.pal[i] = static_cast<uint16_t>(
                std::stoul(tok[1], nullptr, 16));
        } else if (key == "TILE") {
            if (tok.size() != 9) snap_error("TILE needs n plus 8 rows");
            const unsigned n = std::stoul(tok[0]);
            if (n > 255) snap_error("TILE index out of range");
            for (unsigned r = 0; r < 8; ++r) {
                std::array<uint8_t, 8> px{};
                decode_tile_row(
                    static_cast<uint16_t>(std::stoul(tok[1 + r], nullptr,
                                                     16)),
                    &px);
                for (unsigned cI = 0; cI < 8; ++cI) {
                    s.tiles[n][r * 8u + cI] = px[cI];
                }
            }
        } else if (key == "SCROLL") {
            if (tok.size() != 2) snap_error("SCROLL needs 2 args");
            s.scroll_x = std::stoul(tok[0]);
            s.scroll_y = std::stoul(tok[1]);
        } else if (key == "WINDOW") {
            if (tok.size() != 4) snap_error("WINDOW needs 4 args");
            s.win_left = std::stoul(tok[0]);
            s.win_right = std::stoul(tok[1]);
            s.win_enable = tok[2] == "1";
            s.win_invert = tok[3] == "1";
        } else if (key == "MATH") {
            if (tok.size() != 3) snap_error("MATH needs 3 args");
            s.math_sub = tok[0] == "sub";
            s.math_half = tok[1] == "1";
            s.math_on = tok[2] == "1";
        } else if (key == "BACKDROP") {
            if (tok.size() != 1) snap_error("BACKDROP needs 1 arg");
            const unsigned i = std::stoul(tok[0]);
            if (i > 255) snap_error("BACKDROP index out of range");
            s.backdrop = i;
        } else {
            snap_error("unknown key " + key);
        }
    }
    if (s.map.empty()) {
        snap_error("missing SIZE/MAP");
    }
    return s;
}

}  // namespace snesbus
