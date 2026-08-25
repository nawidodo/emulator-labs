#pragma once
#include <cstdint>
#include <array>

// Chapter 22 — sprite evaluation (simplified, documented) and sprite 0 hit.
//
// Real hardware fills secondary OAM dot-by-dot on cycles 65-256 of each
// visible scanline. For frame rendering we only need its OBSERVABLE
// result: which sprites (first 8 in Y range) are drawn on a line, whether
// a 9th was found (overflow flag), and whether sprite 0 is among them.
//
namespace nes22sprite {

struct OamEntry {
    uint8_t y, tile, attr, x;
};

inline OamEntry oam_get(const uint8_t* oam, int i) {
    const uint8_t* e = oam + i * 4;
    return {e[0], e[1], e[2], e[3]};
}

struct EvalResult {
    std::array<int, 8> slots;   // OAM indices of the <=8 sprites on this line
    int count = 0;              // number found (0-8)
    bool sprite0 = false;       // sprite 0 among them?
};

// Clean-model overflow scan: after 8 sprites are found, keep scanning the
// remaining entries with a plain Y-range test.
//
//@LABS-BEGIN 1
//@LABS-SOLUTION
inline EvalResult evaluate(const uint8_t* oam, int line, bool tall_sprites,
                           bool& overflow) {
    EvalResult r;
    const int h = tall_sprites ? 16 : 8;
    for (int i = 0; i < 64; ++i) {
        uint8_t y = oam[i * 4];
        bool in_range = line >= y && line < y + h;
        if (!in_range) continue;
        if (r.count < 8) {
            if (i == 0) r.sprite0 = true;
            r.slots[r.count++] = i;
        } else {
            overflow = true;      // clean model: any 9th in-range sprite
            break;
        }
    }
    return r;
}
//@LABS-STUB
// TODO(1): scan all 64 entries in order; an entry is in range when
// line >= y && line < y + (tall ? 16 : 8). Record the first 8 indices,
// flag sprite 0's presence, and set overflow when MORE than 8 are in
// range. Stub finds nothing (wrong on purpose).
inline EvalResult evaluate(const uint8_t* /*oam*/, int /*line*/,
                           bool /*tall_sprites*/, bool& overflow) {
    overflow = false;
    return {};
}
//@LABS-END

// Hardware-quirk variant (toggle): on real hardware the secondary-OAM write
// counter keeps ticking during overflow evaluation, so the byte compared
// against the scanline is read from the WRONG OAM address: entry index m
// advances once per FOUR reads instead of once per sprite, while the read
// phase p cycles through the four bytes of an entry. Simplified faithful
// model: compare `line` against oam[4*m + p] where p = reads % 4 and m only
// advances when p wraps. This can report overflow for sprites that are NOT
// in range (and miss ones that are), exactly the class of false positives
// games like Tetris battle.
//
//@LABS-BEGIN 2
//@LABS-SOLUTION
inline bool overflow_with_quirk(const uint8_t* oam, int line, int start_m,
                                bool tall_sprites) {
    const int h = tall_sprites ? 16 : 8;
    int phase = 0;
    for (int m = start_m, reads = 0; m < 64 && reads < 64 * 4; ++reads) {
        // The value compared against the scanline drifts through the
        // tile/attr/X bytes as the counter misaligns.
        uint8_t val = oam[(m * 4) + (reads % 4)];
        if (line >= val && line < val + h) return true;
        if (++phase == 4) {   // primary-OAM pointer advances every 4 reads
            phase = 0;
            ++m;
        }
    }
    return false;
}
//@LABS-STUB
// TODO(2): implement the buggy scan described above: iterate reads from
// start_m, comparing line against oam[m*4 + reads%4], advancing m only
// every fourth read; return true on first "in range". Stub returns false.
inline bool overflow_with_quirk(const uint8_t* /*oam*/, int /*line*/,
                                int /*start_m*/, bool /*tall_sprites*/) {
    return false;  // wrong on purpose
}
//@LABS-END

// Sprite 0 hit, EXACT conditions. Called per pixel during rendering:
//   bg_opaque / sp_opaque: both pixels non-zero (after palette decode)
//   x: pixel position 0-255
//   mask bits from PPUMASK: bit1 show bg in leftmost 8px,
//                           bit2 show sprites in leftmost 8px,
//                           bit3 show bg at all, bit4 show sprites at all
// PPUMASK-derived visibility flags shared by the hit predicate.
struct MaskBits {
    bool bg_left_clip;     // PPUMASK bit1 clear -> left 8px of bg hidden
    bool spr_left_clip;    // PPUMASK bit2 clear -> left 8px of sprites hidden
    bool bg_enabled;       // PPUMASK bit3
    bool spr_enabled;      // PPUMASK bit4
};

//@LABS-BEGIN 3
//@LABS-SOLUTION
inline bool sprite0_hit_at(int x, bool bg_opaque, bool sp_opaque,
                           bool line_has_sprite0, const MaskBits& m) {
    if (!m.bg_enabled || !m.spr_enabled) return false;
    if (!bg_opaque || !sp_opaque) return false;
    if (x == 255) return false;
    if (x < 8 && (m.bg_left_clip || m.spr_left_clip)) return false;
    return line_has_sprite0;
}
//@LABS-STUB
// TODO(3): implement the exact condition list above. Stub never reports a
// hit so sprite-0 tests run RED.
inline bool sprite0_hit_at(int /*x*/, bool /*bg_opaque*/, bool /*sp_opaque*/,
                           bool /*line_has_sprite0*/, const MaskBits& /*m*/) {
    return false;  // wrong on purpose
}
//@LABS-END

}  // namespace nes22sprite
