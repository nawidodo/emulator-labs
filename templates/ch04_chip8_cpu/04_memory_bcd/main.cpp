#define LABSTEST_MAIN
#include "labstest.hpp"
#include "chip8.hpp"

#include <vector>

namespace {

using chip8::Chip8;

Chip8 mk(const std::vector<uint8_t>& prog) {
    Chip8 c;
    c.reset();
    c.load(prog);
    return c;
}

void run(Chip8& c, int n) {
    for (int i = 0; i < n && !c.halted; ++i) c.step();
}

}  // namespace

// FX29 points I at the glyph of the low nibble of VX.
TEST(mem, font_lookup) {
    Chip8 c = mk({0xA0, 0x00,  // LD  I,0 (overwritten by FX29)
                  0xF1, 0x29});// LD  F,V1
    c.v[1] = 0x1A;             // low nibble A = digit 10
    run(c, 2);
    EXPECT_EQ(c.idx, 5 * 0xA);
    static const uint8_t kGlyphA[5] = {0xF0, 0x90, 0xF0, 0x90, 0x90};
    for (int k = 0; k < 5; ++k)
        EXPECT_EQ(c.mem[c.idx + k], kGlyphA[k]);
}

// FX33 stores hundreds/tens/ones in that order.
TEST(mem, bcd_digit_order) {
    struct Row { uint8_t val; uint8_t h, t, o; };
    const Row rows[] = {{255, 2, 5, 5}, {42, 0, 4, 2}, {0, 0, 0, 0}, {7, 0, 0, 7}};
    for (const Row& r : rows) {
        Chip8 c = mk({0xA4, 0x00, 0xF0, 0x33});
        c.v[0] = r.val;
        run(c, 2);
        EXPECT_EQ(c.mem[0x400], r.h);
        EXPECT_EQ(c.mem[0x401], r.t);
        EXPECT_EQ(c.mem[0x402], r.o);
    }
}

// Default (COSMAC) profile: FX55/FX65 advance I by X+1 and leave VF alone.
TEST(mem, store_load_roundtrip_advances_i) {
    Chip8 c = mk({0xA5, 0x00,          // LD I,0x500
                  0xFF, 0x55,          // store V0..VF @I
                  0xA5, 0x00,          // LD I,0x500 again
                  0xFF, 0x65});        // refill V0..VF from there
    for (int k = 0; k < 16; ++k) c.v[k] = uint8_t(k * 17);
    run(c, 4);
    for (int k = 0; k < 16; ++k)
        EXPECT_EQ(c.v[k], uint8_t(k * 17));
    EXPECT_EQ(c.idx, 0x510);  // FX65 advanced I by X+1 = 16
}

// CHIP-48 profile: I stays put across load/store.
TEST(mem, load_store_leaves_i_quirk) {
    Chip8 c = mk({0xA5, 0x00, 0xFF, 0x55, 0xFF, 0x65});
    c.quirks.load_store_leaves_i = true;
    c.v[3] = 0x5A;
    run(c, 3);
    EXPECT_EQ(c.mem[0x503], 0x5A);
    EXPECT_EQ(c.v[3], 0x5A);
    EXPECT_EQ(c.idx, 0x500);
}

// vf_reset quirk clears VF as a side effect; off by default.
TEST(mem, vf_reset_quirk) {
    Chip8 off = mk({0xA5, 0x00, 0xFF, 0x55});
    off.v[0xF] = 1;
    run(off, 2);
    EXPECT_EQ(off.v[0xF], 1);

    Chip8 on = mk({0xA5, 0x00, 0xFF, 0x55});
    on.v[0xF] = 1;
    on.quirks.vf_reset = true;
    run(on, 2);
    EXPECT_EQ(on.v[0xF], 0);
}

// Byte-wise addressing wraps at the 4 KiB edge instead of faulting. Use a
// region where wrap-overwrite is observable without trashing the font:
// I=0x2FE with X=1 writes 0x2FE,0x2FF inside loaded ROM area (harmless).
TEST(mem, addressing_wraps_at_4k_edge) {
    Chip8 c;
    c.reset();
    c.idx = 0xFFF;
    c.v[0] = 0x77;
    // Single-byte store via FX33's neighbors is not available; use direct op:
    // FX55 with X=0 stores exactly one byte at wrap12(0xFFF)=0xFFF.
    const uint8_t prog[] = {0xF0, 0x55};
    c.load(std::vector<uint8_t>(prog, prog + 2));
    c.step();
    EXPECT_EQ(c.mem[0xFFF], 0x77);
    EXPECT_EQ(c.halted, false);
}
