#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include "exec_ir.hpp"
#include "opt.hpp"

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
        rx8::enc(rx8::OP_ADD, 3, 1, 0),      // identity copy bait
        rx8::enc(rx8::OP_MOV, 4, 3),         // dead copy bait
        rx8::enc(rx8::OP_ADD, 5, 4, 1),
        rx8::enc(rx8::OP_SW, 0, 5, 0, 512),
        rx8::enc(rx8::OP_ADDI, 2, 0, 0, 1),  // dead constant bait
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
        rx8::enc(rx8::OP_SW, 0, 3, 0, 8),
        rx8::enc(rx8::OP_JMP, 0, 0, 0, 0x08 / 4),
        rx8::enc(rx8::OP_HALT),
    });
}

}  // namespace

TEST(opt, identity_folds_rewrite_insns) {
    auto img = to_image({
        rx8::enc(rx8::OP_ADDI, 1, 0, 0, 7),   // li r1,7 candidate
        rx8::enc(rx8::OP_XOR, 2, 3, 3),       // xor-self -> zero
        rx8::enc(rx8::OP_SUB, 4, 5, 0),       // x - r0 -> mov
        rx8::enc(rx8::OP_HALT),
    });
    rx8::Machine m;
    m.load(img);
    auto blocks = rx8::translate(m, rx8::find_blocks(m, uint32_t(img.size())));
    EXPECT_EQ(blocks.size(), size_t{1});
    rx8::fold_identities(blocks);
    const auto& ins = blocks[0].insns;
    EXPECT_TRUE(ins[0].op == rx8::IrOp::Li && ins[0].imm12 == 7);
    EXPECT_TRUE(ins[1].op == rx8::IrOp::Li && ins[1].imm12 == 0);
    EXPECT_TRUE(ins[2].op == rx8::IrOp::Mov);
}

TEST(opt, copy_prop_exposes_dead_bait_on_bench) {
    auto img = bench_image();
    rx8::Machine m;
    m.load(img);
    auto blocks = rx8::translate(m, rx8::find_blocks(m, uint32_t(img.size())));
    rx8::optimize(blocks);

    // The loop block (entry 0x0c) must shrink to exactly:
    // add r1,r1,r6 ; add r5,r1,r1 ; store ; decbr r7,-2.
    const rx8::IrBlock* loop = nullptr;
    for (const auto& b : blocks)
        if (b.entry == 0x0c) loop = &b;
    EXPECT_TRUE(loop != nullptr);
    if (loop == nullptr || loop->insns.size() != 4) return;
    EXPECT_TRUE(loop->insns[0].op == rx8::IrOp::Alu);
    EXPECT_TRUE(loop->insns[2].op == rx8::IrOp::Store);
    EXPECT_TRUE(loop->insns[3].op == rx8::IrOp::DecBr);
    EXPECT_EQ(loop->insns[3].rd, uint8_t{7});
    EXPECT_EQ(loop->insns[3].simm(), -2);
    // Setup kept only the three live constants... as Li ops.
    const rx8::IrBlock* setup = &blocks[0];
    EXPECT_EQ(setup->entry, uint32_t{0});
    EXPECT_EQ(setup->insns.size(), size_t{3});
}

TEST(opt, no_fusion_when_branch_does_not_match) {
    auto img = to_image({
        rx8::enc(rx8::OP_ADDI, 1, 1, 0, -1 & 0xFFF),
        rx8::enc(rx8::OP_BEQZ, 0, 1, 0, 0x08 / 4),  // branch-if-ZERO: no fuse
        rx8::enc(rx8::OP_HALT),
    });
    rx8::Machine m;
    m.load(img);
    auto blocks = rx8::translate(m, rx8::find_blocks(m, uint32_t(img.size())));
    rx8::fuse_dec_branch(blocks);
    EXPECT_TRUE(blocks[0].insns.back().op == rx8::IrOp::Br);
}

TEST(opt, bench_clears_20pct_with_identical_dump) {
    auto img = bench_image();

    rx8::Machine ref;
    ref.load(img);
    rx8::run(ref, 100000);

    rx8::IrEngine base;
    base.load(img);
    base.run(1000000);

    rx8::Machine m;
    m.load(img);
    auto blocks =
        rx8::translate(m, rx8::find_blocks(m, uint32_t(img.size())));
    rx8::optimize(blocks);

    rx8::IrEngine opt;
    opt.load(img);
    opt.install(std::move(blocks));
    opt.run(1000000);

    EXPECT_EQ(rx8::observable_dump(opt.m), rx8::observable_dump(base.m));
    EXPECT_EQ(rx8::observable_dump(opt.m), rx8::observable_dump(ref));
    // >= 20% reduction, integer-exact: optimized*5 <= baseline*4.
    EXPECT_TRUE(opt.ops_executed * 5 <= base.ops_executed * 4);
}

TEST(opt, smc_output_preserved_under_optimization) {
    auto img = smc_image();
    rx8::Machine ref;
    ref.load(img);
    rx8::run(ref, 100000);

    rx8::Machine m;
    m.load(img);
    auto blocks =
        rx8::translate(m, rx8::find_blocks(m, uint32_t(img.size())));
    rx8::optimize(blocks);
    rx8::IrEngine opt;
    opt.load(img);
    opt.install(std::move(blocks));
    opt.run(1000000);

    EXPECT_TRUE(!opt.m.fault);
    EXPECT_TRUE(opt.m.halted);
    EXPECT_EQ(rx8::observable_dump(opt.m), rx8::observable_dump(ref));
}
