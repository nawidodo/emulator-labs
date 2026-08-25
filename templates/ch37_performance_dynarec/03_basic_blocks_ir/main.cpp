#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include "exec_ir.hpp"

namespace {

std::vector<uint8_t> to_image(const std::vector<uint32_t>& words) {
    std::vector<uint8_t> img;
    for (uint32_t w : words)
        for (int b = 0; b < 4; ++b) img.push_back(uint8_t(w >> (8 * b)));
    return img;
}

// bench.bin of tests/public/ch37_performance_dynarec/programs/.
std::vector<uint8_t> bench_image() {
    return to_image({
        rx8::enc(rx8::OP_ADDI, 7, 0, 0, 12),
        rx8::enc(rx8::OP_ADDI, 6, 0, 0, 3),
        rx8::enc(rx8::OP_ADDI, 1, 0, 0, 0),
        // loop @ 0x0c
        rx8::enc(rx8::OP_ADD, 1, 1, 6),
        rx8::enc(rx8::OP_ADD, 3, 1, 0),
        rx8::enc(rx8::OP_MOV, 4, 3),
        rx8::enc(rx8::OP_ADD, 5, 4, 1),
        rx8::enc(rx8::OP_SW, 0, 5, 0, 512),
        rx8::enc(rx8::OP_ADDI, 2, 0, 0, 1),
        rx8::enc(rx8::OP_ADDI, 7, 7, 0, -2 & 0xFFF),
        rx8::enc(rx8::OP_BNEZ, 0, 7, 0, 0x0c / 4),
        rx8::enc(rx8::OP_OUT, 1),
        rx8::enc(rx8::OP_HALT),
    });
}

// smc.bin: patches its own out-instruction after pass 1.
std::vector<uint8_t> smc_image() {
    return to_image({
        rx8::enc(rx8::OP_ADDI, 4, 0, 0, 2),
        rx8::enc(rx8::OP_ADDI, 2, 0, 0, 111),
        rx8::enc(rx8::OP_OUT, 2),                    // site @ 0x08
        rx8::enc(rx8::OP_BEQZ, 0, 4, 0, 0x20 / 4),
        rx8::enc(rx8::OP_ADDI, 4, 4, 0, -1 & 0xFFF),
        rx8::enc(rx8::OP_ADDI, 3, 0, 0, 9),
        rx8::enc(rx8::OP_SW, 0, 3, 0, 8),            // patch mem[8]
        rx8::enc(rx8::OP_JMP, 0, 0, 0, 0x08 / 4),
        rx8::enc(rx8::OP_HALT),
    });
}

}  // namespace

TEST(blocks, leaders_terminators_and_links) {
    // 00 addi r1,r0,3 ; 04 addi r1,r1,-1 ; 08 bnez r1->04 ; 0c halt
    auto img = to_image({
        rx8::enc(rx8::OP_ADDI, 1, 0, 0, 3),
        rx8::enc(rx8::OP_ADDI, 1, 1, 0, -1 & 0xFFF),
        rx8::enc(rx8::OP_BNEZ, 0, 1, 0, 0x04 / 4),
        rx8::enc(rx8::OP_HALT),
    });
    rx8::Machine m;
    m.load(img);
    const auto blocks = rx8::find_blocks(m, uint32_t(img.size()));
    // 0x04 is a branch TARGET and therefore a leader even without any
    // terminator before it — the entry run must be cut there.
    EXPECT_EQ(blocks.size(), size_t{3});
    if (blocks.size() != 3) return;
    EXPECT_EQ(blocks[0].start, uint32_t{0x00});
    EXPECT_EQ(blocks[0].term_pc, uint32_t{0x00});
    EXPECT_EQ(blocks[0].fallthrough, uint32_t{0x04});
    EXPECT_EQ(blocks[1].start, uint32_t{0x04});
    EXPECT_EQ(blocks[1].term_pc, uint32_t{0x08});
    EXPECT_EQ(blocks[1].taken, uint32_t{0x04});
    EXPECT_EQ(blocks[1].fallthrough, uint32_t{0x0c});
    EXPECT_EQ(blocks[2].start, uint32_t{0x0c});
    EXPECT_EQ(blocks[2].term_pc, uint32_t{0x0c});
}

TEST(blocks, jmp_has_no_fallthrough) {
    auto img = to_image({
        rx8::enc(rx8::OP_JMP, 0, 0, 0, 0x08 / 4),
        rx8::enc(rx8::OP_NOP),
        rx8::enc(rx8::OP_OUT, 1),
        rx8::enc(rx8::OP_HALT),
    });
    rx8::Machine m;
    m.load(img);
    const auto blocks = rx8::find_blocks(m, uint32_t(img.size()));
    EXPECT_EQ(blocks.size(), size_t{3});
    if (blocks.size() != 3) return;
    EXPECT_EQ(blocks[0].taken, uint32_t{0x08});
    EXPECT_EQ(blocks[0].fallthrough, rx8::kNoLink);  // jmp never falls through
    EXPECT_EQ(blocks[1].start, uint32_t{0x04});      // after-jmp leader
    EXPECT_EQ(blocks[1].fallthrough, uint32_t{0x08});
    EXPECT_EQ(blocks[2].term_pc, uint32_t{0x0c});    // jump-target block ends in halt
}

TEST(ir, lowering_shapes_mirror_guest_fields) {
    const rx8::IrInsn mov =
        rx8::lower_insn(rx8::decode(rx8::enc(rx8::OP_MOV, 4, 2)));
    EXPECT_TRUE(mov.op == rx8::IrOp::Mov);

    const rx8::IrInsn addi =
        rx8::lower_insn(rx8::decode(rx8::enc(rx8::OP_ADDI, 1, 2, 0, 5)));
    EXPECT_TRUE(addi.op == rx8::IrOp::Alu);
    EXPECT_TRUE(addi.use_imm);
    EXPECT_EQ(addi.simm(), 5);

    const rx8::IrInsn sw =
        rx8::lower_insn(rx8::decode(rx8::enc(rx8::OP_SW, 3, 5, 0, 64)));
    EXPECT_TRUE(sw.op == rx8::IrOp::Store);
    EXPECT_EQ(sw.rd, uint8_t{3});  // base register stays in rd
    EXPECT_EQ(sw.rs, uint8_t{5});  // stored source stays in rs

    const rx8::IrInsn bnez =
        rx8::lower_insn(rx8::decode(rx8::enc(rx8::OP_BNEZ, 0, 7, 0, 12 / 4)));
    EXPECT_TRUE(bnez.op == rx8::IrOp::Br);
    EXPECT_EQ(bnez.br_kind, uint8_t{0});
    EXPECT_EQ(bnez.target, uint32_t{12});

    const rx8::IrInsn bad =
        rx8::lower_insn(rx8::decode(rx8::enc(0x77)));
    EXPECT_TRUE(bad.op == rx8::IrOp::Undef);
}

TEST(pipeline, ir_matches_switch_on_bench) {
    auto img = bench_image();

    rx8::Machine ref;
    ref.load(img);
    rx8::run(ref, 100000);

    rx8::IrEngine eng;
    eng.load(img);
    eng.run(1000000);

    EXPECT_TRUE(eng.m.halted);
    EXPECT_TRUE(!eng.m.fault);
    EXPECT_EQ(rx8::observable_dump(eng.m), rx8::observable_dump(ref));
    EXPECT_EQ(eng.m.out.size(), size_t{1});
    if (eng.m.out.size() == 1) EXPECT_EQ(eng.m.out[0], uint32_t{18});
    // One IR op per guest instruction in the unoptimized pipeline.
    EXPECT_EQ(eng.ops_executed, uint64_t{53});
}

TEST(pipeline, ir_flushes_stale_blocks_on_smc) {
    auto img = smc_image();

    rx8::Machine ref;
    ref.load(img);
    rx8::run(ref, 100000);

    rx8::IrEngine eng;
    eng.load(img);
    eng.run(1000000);

    EXPECT_TRUE(eng.m.halted);
    EXPECT_EQ(rx8::observable_dump(eng.m), rx8::observable_dump(ref));
    // patched-away OUT never re-fires:
    EXPECT_EQ(eng.m.out.size(), size_t{1});
    if (eng.m.out.size() == 1) EXPECT_EQ(eng.m.out[0], uint32_t{111});
    EXPECT_TRUE(!eng.m.fault);
    EXPECT_TRUE(eng.flushes > 0);  // the store really dropped translations
}
