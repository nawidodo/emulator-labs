#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "../01_loopy_scroll/loopy.hpp"

// Chapter 22 challenge — a dot-accurate PPU timing model for scanline/dot
// snapshots. Course-original: it models only the loopy-register side of
// rendering, which is what scroll-sensitive raster effects depend on.
//
// Hardware rules implemented (when rendering is enabled), on visible and
// pre-render lines:
//   dots 8,16,...,256, 328, 336 : increment_x (tile fetch boundaries)
//   dot 256                     : increment_y
//   dot 257                     : copy_x
//   pre-render line 261, dots 280-304: copy_y every dot
//
// The model advances one dot at a time; register writes queued for a
// (line, dot) are applied just before that dot's fetch action.
namespace nes22timing {

struct PpuTiming {
    nes22scroll::Loopy l;
    int line = 0;    // 0-261 (261 = pre-render)
    int dot = 0;     // 0-340; "current" position, advance moves to next
    bool rendering = true;
};

inline void apply_dot_actions(PpuTiming& p) {
    if (!p.rendering) return;
    const bool fetch_line = p.line < 240 || p.line == 261;
    if (!fetch_line) return;

    const bool prerender = p.line == 261;
    const int d = p.dot;
    if ((d % 8 == 0 && d >= 8 && d <= 256) || d == 328 || d == 336)
        nes22scroll::increment_x(p.l);
    if (d == 256) nes22scroll::increment_y(p.l);
    if (d == 257) nes22scroll::copy_x(p.l);
    if (prerender && d >= 280 && d <= 304) nes22scroll::copy_y(p.l);
}

inline void tick(PpuTiming& p) {
    p.dot++;
    if (p.dot > 340) {
        p.dot = 0;
        p.line = (p.line + 1) % 262;
    }
    apply_dot_actions(p);
}

inline void run_to(PpuTiming& p, int line, int dot) {
    while (p.line != line || p.dot != dot) {
        if (p.line > line || (p.line == line && p.dot > dot))
            return;  // target already passed; never rewind
        tick(p);
    }
}

inline std::string snapshot_text(const PpuTiming& p) {
    char buf[96];
    std::snprintf(buf, sizeof(buf),
                  "line=%d dot=%d v=%04X x=%u w=%d", p.line, p.dot,
                  unsigned(p.l.v), unsigned(p.l.x), p.l.w ? 1 : 0);
    return buf;
}

}  // namespace nes22timing
