#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include "ext.hpp"

namespace {

std::vector<uint8_t> to_image(const std::vector<uint32_t>& words) {
    std::vector<uint8_t> img;
    for (uint32_t w : words)
        for (int b = 0; b < 4; ++b) img.push_back(uint8_t(w >> (8 * b)));
    return img;
}

// Reference loop: switch interpreter with the extension tier consulted
// first (the student's execute_ext under test).
uint64_t run_interp(rx8::Machine& m, uint64_t max_steps) {
    uint64_t n = 0;
    while (n < max_steps && !m.halted && !m.fault) {
        const uint32_t at = m.pc;
        m.pc += 4;
        const rx8::Decoded d = rx8::decode(m.load_word(at));
        if (!rx8ext::execute_ext(m, d)) rx8::execute(m, d);
        ++m.executed;
        ++n;
    }
    return n;
}

std::vector<uint8_t> ext_image() {
    // The ext1 fixture shape: mul/not/min inside a countdown loop.
    return to_image({
        rx8::enc(rx8::OP_ADDI, 1, 0, 0, 6),
        rx8::enc(rx8::OP_ADDI, 2, 0, 0, 7),
        // loop @ 0x08
        rx8::enc(rx8ext::OP_MUL, 3, 1, 2),   // r3 = r1 * r2
        rx8::enc(rx8ext::OP_NOT, 4, 3),      // r4 = ~r3
        rx8::enc(rx8ext::OP_MIN, 5, 1, 4),   // r5 = signed min(r1, r4)
        rx8::enc(rx8::OP_OUT, 5),
        rx8::enc(rx8::OP_ADDI, 1, 1, 0, -1 & 0xFFF),
        rx8::enc(rx8::OP_BNEZ, 0, 1, 0, 0x08 / 4),
        rx8::enc(rx8::OP_OUT, 3),            // last product = 1*7
        rx8::enc(rx8::OP_HALT),
    });
}

}  // namespace

TEST(ext, interpreter_semantics) {
    rx8::Machine m;
    m.r[2] = 7;
    m.r[3] = 6;
    EXPECT_TRUE(rx8ext::execute_ext(
        m, rx8::decode(rx8::enc(rx8ext::OP_MUL, 1, 2, 3))));
    EXPECT_EQ(m.r[1], uint32_t{42});
    EXPECT_TRUE(rx8ext::execute_ext(
        m, rx8::decode(rx8::enc(rx8ext::OP_NOT, 4, 1))));
    EXPECT_EQ(m.r[4], uint32_t{0xFFFFFFD5u});  // -41
    EXPECT_TRUE(rx8ext::execute_ext(
        m, rx8::decode(rx8::enc(rx8ext::OP_MIN, 5, 2, 4))));  // min(7,-41)
    EXPECT_EQ(m.r[5], uint32_t{0xFFFFFFD5u});
    // Wrapping low-32 product.
    m.r[2] = 0x10000u;
    m.r[3] = 0x10000u;
    rx8ext::execute_ext(m, rx8::decode(rx8::enc(rx8ext::OP_MUL, 6, 2, 3)));
    EXPECT_EQ(m.r[6], uint32_t{0});
    // Foreign opcodes fall through.
    EXPECT_TRUE(!rx8ext::execute_ext(m, rx8::decode(rx8::enc(rx8::OP_ADD, 1, 2, 3))));
}

TEST(ext, r0_rule_holds_for_extensions) {
    rx8::Machine m;
    m.r[1] = 9;
    rx8ext::execute_ext(m, rx8::decode(rx8::enc(rx8ext::OP_MUL, 0, 1, 1)));
    EXPECT_EQ(m.r[0], uint32_t{0});
    rx8ext::execute_ext(m, rx8::decode(rx8::enc(rx8ext::OP_MIN, 0, 1, 0)));
    EXPECT_EQ(m.r[0], uint32_t{0});
}

TEST(ext, lowering_uses_extended_alu_kinds) {
    rx8::IrInsn out;
    EXPECT_TRUE(rx8ext::lower_ext(
        rx8::decode(rx8::enc(rx8ext::OP_MUL, 3, 1, 2)), out));
    EXPECT_TRUE(out.op == rx8::IrOp::Alu);
    EXPECT_EQ(static_cast<uint8_t>(out.alu),
              static_cast<uint8_t>(rx8ext::AluExt::Mul));
    EXPECT_TRUE(!out.use_imm);

    rx8::IrInsn not_insn;
    EXPECT_TRUE(rx8ext::lower_ext(
        rx8::decode(rx8::enc(rx8ext::OP_NOT, 4, 3)), not_insn));
    EXPECT_EQ(static_cast<uint8_t>(not_insn.alu),
              static_cast<uint8_t>(rx8ext::AluExt::Not));

    rx8::IrInsn plain;
    EXPECT_TRUE(!rx8ext::lower_ext(
        rx8::decode(rx8::enc(rx8::OP_MOV, 1, 2)), plain));
}

TEST(ext, pipeline_matches_interpreter_bit_exact) {
    auto img = ext_image();

    rx8::Machine ref;
    ref.load(img);
    run_interp(ref, 100000);
    EXPECT_EQ(ref.out.size(), size_t{7});
    if (ref.out.size() == 7) {
        EXPECT_EQ(ref.out[0], uint32_t{0xFFFFFFD5u});  // min(6, ~42)
        EXPECT_EQ(ref.out[5], uint32_t{0xFFFFFFF8u});  // min(1, ~7)
        EXPECT_EQ(ref.out[6], uint32_t{7});            // final product
    }

    const auto plain = rx8ext::run_ext(img, false, 1000000);
    const auto opt = rx8ext::run_ext(img, true, 1000000);
    EXPECT_TRUE(plain.halted && !plain.fault);
    EXPECT_TRUE(opt.halted && !opt.fault);
    EXPECT_EQ(plain.dump, rx8::observable_dump(ref));
    EXPECT_EQ(opt.dump, plain.dump);  // optimizer preserves extensions
}
