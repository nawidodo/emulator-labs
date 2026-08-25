// timing.hpp — explicit dot-driven PPU timing state machine.
//
// The DMG PPU runs on a 456-dot line clock; a frame is 154 lines, i.e.
// 70224 dots (~59.7 Hz). Lines 0..143 are visible, 144..153 are vblank.
//
// Course simplification: real mode 3 lasts 172..289 dots depending on
// sprite fetches; we use FIXED mode widths so the model stays pure and
// hashable:
//   mode 2 (OAM scan):  dots   0..79
//   mode 3 (drawing) :  dots  80..251
//   mode 0 (hblank)  :  dots 252..455
//   mode 1 (vblank)  :  all of lines 144..153
#pragma once

#include <cstdint>
#include <string>

namespace gbtim {

enum class Mode : uint8_t {
    HBlank = 0,
    VBlank = 1,
    OamScan = 2,
    Drawing = 3,
};

constexpr int kDotsPerLine = 456;
constexpr int kVisibleLines = 144;
constexpr int kTotalLines = 154;
constexpr int kFrameDots = kDotsPerLine * kTotalLines;  // 70224
constexpr int kMode2End = 80;   // exclusive upper bound of OAM scan
constexpr int kMode3End = 252;  // exclusive upper bound of drawing

struct PpuTiming {
    int ly = 0;
    int dot = 0;
    Mode mode = Mode::OamScan;
};

// Pure mode lookup: which mode is the PPU in on `line` at `dot`?
inline Mode modeAt(int line, int dot) {
//@LABS-BEGIN 1
//@LABS-SOLUTION
    if (line >= kVisibleLines) return Mode::VBlank;
    if (dot < kMode2End) return Mode::OamScan;
    if (dot < kMode3End) return Mode::Drawing;
    return Mode::HBlank;
//@LABS-STUB
    // TODO(1): vblank covers whole lines 144..153; otherwise split each
    // line at dot 80 and dot 252 into modes 2 / 3 / 0.
    (void)line;
    (void)dot;
    return Mode::HBlank;
//@LABS-END
}

// Advance the machine by `dots` ticks, carrying across line and frame
// boundaries. LY wraps 153 -> 0 and dot wraps at 456, so advancing by
// exactly kFrameDots returns an identical state.
inline PpuTiming advance(PpuTiming t, int dots) {
//@LABS-BEGIN 2
//@LABS-SOLUTION
    t.dot += dots;
    t.ly = (t.ly + t.dot / kDotsPerLine) % kTotalLines;
    t.dot %= kDotsPerLine;
    t.mode = modeAt(t.ly, t.dot);
    return t;
//@LABS-STUB
    // TODO(2): add dots, carry into LY at every 456 boundary with wrap at
    // 154 lines, then refresh the mode via modeAt.
    (void)dots;
    return t;
//@LABS-END
}

// VRAM is inaccessible to the CPU during mode 3.
inline bool vramLocked(int line, int dot) {
//@LABS-BEGIN 3
//@LABS-SOLUTION
    return line < kVisibleLines && modeAt(line, dot) == Mode::Drawing;
//@LABS-STUB
    // TODO(3): true exactly in mode 3 on a visible line.
    (void)line;
    (void)dot;
    return false;
//@LABS-END
}

// OAM is inaccessible during modes 2 AND 3.
inline bool oamLocked(int line, int dot) {
//@LABS-BEGIN 4
//@LABS-SOLUTION
    if (line >= kVisibleLines) return false;
    const Mode m = modeAt(line, dot);
    return m == Mode::OamScan || m == Mode::Drawing;
//@LABS-STUB
    // TODO(4): true in mode 2 or mode 3 on a visible line.
    (void)line;
    (void)dot;
    return false;
//@LABS-END
}

// Deterministic mode-transition trace: one line "ly=<n> dot=<n> mode=<m>\n"
// per change of (ly, mode) while stepping `totalDots` single dots from the
// top of frame. The chapter runner writes the same format for its --trace
// file; golden copies live under tests/public/ch15_gameboy_ppu2/traces/.
inline std::string buildModeTrace(int totalDots) {
//@LABS-BEGIN 5
//@LABS-SOLUTION
    std::string out;
    int prevLy = -1;
    int prevMode = -1;
    for (int d = 0; d < totalDots; ++d) {
        const int ly = d / kDotsPerLine;
        const int dot = d % kDotsPerLine;
        const int m = static_cast<int>(modeAt(ly, dot));
        if (ly != prevLy || m != prevMode) {
            out += "ly=" + std::to_string(ly) +
                   " dot=" + std::to_string(dot) +
                   " mode=" + std::to_string(m) + "\n";
            prevLy = ly;
            prevMode = m;
        }
    }
    return out;
//@LABS-STUB
    // TODO(5): step single dots, emit one "ly=N dot=N mode=M\n" line per
    // (ly, mode) change — see the comment above for the exact format.
    (void)totalDots;
    return "";
//@LABS-END
}

}  // namespace gbtim
