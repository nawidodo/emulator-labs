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

// 6XNN loads; 7XNN adds WITHOUT flags - it wraps mod 256 silently.
TEST(alu, imm_load_and_add_wrap_silently) {
    Chip8 c = mk({0x61, 0xFF,  // LD  V1,0xFF
                  0x71, 0x02,  // ADD V1,2 -> wraps to 1
                  0x74, 0x01});
    run(c, 2);
    EXPECT_EQ(c.v[1], 0x01);
    EXPECT_EQ(c.v[0xF], 0);   // 7XNN never sets VF
    EXPECT_EQ(c.halted, false);
}

TEST(alu, mov_or_and_xor) {
    Chip8 m = mk({0x81, 0x20}); m.v[1] = 0xF0; m.v[2] = 0x3C;
    run(m, 1);
    EXPECT_EQ(m.v[1], 0x3C);
    // OR
    Chip8 o = mk({0x81, 0x21}); o.v[1] = 0xF0; o.v[2] = 0x3C;
    run(o, 1);
    EXPECT_EQ(o.v[1], 0xFC);
    // AND
    Chip8 a = mk({0x81, 0x22}); a.v[1] = 0xF0; a.v[2] = 0x3C;
    run(a, 1);
    EXPECT_EQ(a.v[1], 0x30);
    // XOR
    Chip8 x = mk({0x81, 0x23}); x.v[1] = 0xF0; x.v[2] = 0xFF;
    run(x, 1);
    EXPECT_EQ(x.v[1], 0x0F);
    EXPECT_EQ(x.v[0xF], 0);
}

// 8XY4 carry boundaries: FF+1, 80+80, 7F+1.
TEST(alu, add_carry_boundaries) {
    struct Row { uint8_t a, b, expect_v, expect_f; };
    const Row rows[] = {
        {0xFF, 0x01, 0x00, 1},
        {0x80, 0x80, 0x00, 1},
        {0x7F, 0x01, 0x80, 0},
        {0xAB, 0xCD, 0x78, 1},
    };
    for (const Row& r : rows) {
        const uint8_t prog[] = {0x80, 0xE4};  // ADD V0,VE
        Chip8 c = mk(std::vector<uint8_t>(prog, prog + 2));
        c.v[0] = r.a;
        c.v[0xE] = r.b;
        run(c, 1);
        EXPECT_EQ(c.v[0], r.expect_v);
        EXPECT_EQ(c.v[0xF], r.expect_f);
    }
}

// 8XY5: VF = NOT borrow (1 when the subtraction fit).
TEST(alu, sub_borrow_semantics) {
    struct Row { uint8_t a, b, expect_v, expect_f; };
    const Row rows[] = {
        {0x05, 0x03, 0x02, 1},   // plain
        {0x03, 0x05, 0xFE, 0},   // borrow
        {0x42, 0x42, 0x00, 1},   // equal counts as "no borrow"
        {0x00, 0x01, 0xFF, 0},   // underflow one step
    };
    for (const Row& r : rows) {
        const uint8_t prog[] = {0x80, 0xE5};
        Chip8 c = mk(std::vector<uint8_t>(prog, prog + 2));
        c.v[0] = r.a;
        c.v[0xE] = r.b;
        run(c, 1);
        EXPECT_EQ(c.v[0], r.expect_v);
        EXPECT_EQ(c.v[0xF], r.expect_f);
    }
}

// 8XY7 subtracts THE OTHER WAY (VX = VY - VX) but keeps flag semantics.
TEST(alu, subn_direction) {
    // VX=3, VY=5: result 2, no borrow -> VF=1.
    const uint8_t p1[] = {0x80, 0xE7};
    Chip8 c = mk(std::vector<uint8_t>(p1, p1 + 2));
    c.v[0] = 3; c.v[0xE] = 5;
    run(c, 1);
    EXPECT_EQ(c.v[0], 2);
    EXPECT_EQ(c.v[0xF], 1);

    // VX=5, VY=3: result wraps to FE, borrow -> VF=0.
    const uint8_t p2[] = {0x80, 0xE7};
    Chip8 d = mk(std::vector<uint8_t>(p2, p2 + 2));
    d.v[0] = 5; d.v[0xE] = 3;
    run(d, 1);
    EXPECT_EQ(d.v[0], 0xFE);
    EXPECT_EQ(d.v[0xF], 0);
}

// Flag rules only apply to register ALU forms; imm add left VF alone above.
TEST(alu, logic_ops_leave_vf_alone) {
    const uint8_t prog[] = {0x81, 0x22};  // AND V1,V2
    Chip8 c = mk(std::vector<uint8_t>(prog, prog + 2));
    c.v[0xF] = 1;
    c.v[1] = 0x0F;
    c.v[2] = 0xF0;
    run(c, 1);
    EXPECT_EQ(c.v[1], 0x00);
    EXPECT_EQ(c.v[0xF], 1);
}

// Unassigned 8XY* codes are illegal, not silent no-ops.
TEST(alu, unassigned_alu_codes_halt) {
    const uint8_t prog[] = {0x81, 0x2D};  // 8XYD unassigned
    Chip8 c = mk(std::vector<uint8_t>(prog, prog + 2));
    run(c, 1);
    EXPECT_TRUE(c.halted);
    EXPECT_EQ(c.illegal_op, 0x812D);
}
