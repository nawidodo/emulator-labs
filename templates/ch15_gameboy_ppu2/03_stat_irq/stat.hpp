// stat.hpp — STAT ($FF41) interrupt logic with rising-edge detection.
//
// The STAT line is the OR of up to four enabled sources:
//   - LY == LYC coincidence        (STAT bit 6 enables)
//   - mode == 2, OAM scan          (STAT bit 5)
//   - mode == 1, vblank            (STAT bit 4)
//   - mode == 0, hblank            (STAT bit 3)
//
// DMG quirk: the CPU is interrupted on the RISING EDGE of this OR-ed line,
// not per source. Two sources asserting back-to-back with no low gap
// between them produce ONE interrupt. (Block 0 of STAT, bits 0-2, is read-
// only status and never interrupts by itself.)
#pragma once

#include <cstdint>

namespace gbstat {

constexpr uint8_t kStatLycEnable = 0x40;
constexpr uint8_t kStatOamEnable = 0x20;
constexpr uint8_t kStatVBlankEnable = 0x10;
constexpr uint8_t kStatHBlankEnable = 0x08;
constexpr uint8_t kStatCoincidence = 0x04;

// The OR-ed STAT interrupt line: true when any ENABLED source asserts.
// `coincidence` is the raw LY==LYC result; `mode` is 0..3.
inline bool statSignal(bool lycEnable, bool oamEnable, bool vblankEnable,
                       bool hblankEnable, bool coincidence, int mode) {
//@LABS-BEGIN 1
//@LABS-SOLUTION
    return (lycEnable && coincidence) || (oamEnable && mode == 2) ||
           (vblankEnable && mode == 1) || (hblankEnable && mode == 0);
//@LABS-STUB
    // TODO(1): OR together every source gated by its enable flag.
    (void)lycEnable;
    (void)oamEnable;
    (void)vblankEnable;
    (void)hblankEnable;
    (void)coincidence;
    (void)mode;
    return false;
//@LABS-END
}

// Coincidence flag: LY equals LYC.
inline bool coincidenceFlag(int ly, int lyc) {
//@LABS-BEGIN 2
//@LABS-SOLUTION
    return ly == lyc;
//@LABS-STUB
    // TODO(2): compare ly against lyc.
    (void)ly;
    (void)lyc;
    return false;
//@LABS-END
}

// Rising-edge detector for the OR-ed STAT line. `prev` holds last tick's
// line level, `line` the current one.
struct EdgeDetector {
    bool prev = false;
    bool line = false;
};

// Feed one new sample; returns true exactly on a false->true transition
// (the DMG fires the STAT interrupt on this edge only).
inline bool feed(EdgeDetector& d, bool signal) {
//@LABS-BEGIN 3
//@LABS-SOLUTION
    d.prev = d.line;
    d.line = signal;
    return d.line && !d.prev;
//@LABS-STUB
    // TODO(3): shift line into prev, store signal, report 0->1 edges only.
    (void)d;
    (void)signal;
    return false;
//@LABS-END
}

}  // namespace gbstat
