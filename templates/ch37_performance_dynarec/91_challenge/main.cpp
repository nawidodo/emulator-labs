#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "blocks.hpp"
#include "exec_ir.hpp"
#include "ir.hpp"
#include "opt.hpp"
#include "rx8.hpp"

namespace {

std::vector<uint8_t> to_image(const std::vector<uint32_t>& words) {
    std::vector<uint8_t> img;
    for (uint32_t w : words)
        for (int b = 0; b < 4; ++b) img.push_back(uint8_t(w >> (8 * b)));
    return img;
}

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

struct PipelineResult {
    uint64_t ops = 0;
    std::string dump;
    bool halted = false;
    bool fault = false;
};

}  // namespace

//@LABS-BEGIN 1
//@LABS-SOLUTION
namespace {

// Offline front half of the compiler: analyze control flow, then lower
// every basic block to IR against the machine's CURRENT memory image.
std::vector<rx8::IrBlock> build_ir(const rx8::Machine& m,
                                   uint32_t code_end) {
    return rx8::translate(m, rx8::find_blocks(m, code_end));
}

}  // namespace
//@LABS-STUB
namespace {

// TODO(1): the offline front half of the compiler — run the basic-block
// analyzer over [0, code_end), then translate every block to IR with
// rx8::translate and return it.
std::vector<rx8::IrBlock> build_ir(const rx8::Machine& m,
                                   uint32_t code_end) {
    (void)m;
    (void)code_end;
    return {};  // wrong on purpose: compiles nothing
}

}  // namespace
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
namespace {

// Full pipeline driver: optionally optimize the offline IR, install it,
// execute, and report cost + observable state. Lazy fallback keeps SMC
// correct: stores inside the code region still flush stale translations.
PipelineResult run_pipeline(std::span<const uint8_t> image, bool optimize_ir,
                            uint64_t max_ops) {
    rx8::Machine m;
    m.load(image);
    auto blocks = build_ir(m, uint32_t(image.size()));
    if (optimize_ir) rx8::optimize(blocks);

    rx8::IrEngine eng;
    eng.load(image);
    eng.install(std::move(blocks));
    eng.run(max_ops);

    PipelineResult r;
    r.ops = eng.ops_executed;
    r.dump = rx8::observable_dump(eng.m);
    r.halted = eng.m.halted;
    r.fault = eng.m.fault;
    return r;
}

}  // namespace
//@LABS-STUB
namespace {

// TODO(2): the pipeline driver — load a Machine, call build_ir(), apply
// rx8::optimize() when optimize_ir is set, then load() an IrEngine with
// PipelineResult{ops_executed, observable_dump, halted, fault}.
PipelineResult run_pipeline(std::span<const uint8_t> image, bool optimize_ir,
                            uint64_t max_ops) {
    (void)image;
    (void)optimize_ir;
    (void)max_ops;
    return {};  // wrong on purpose: runs nothing
}

}  // namespace
//@LABS-END

namespace {

void expect_bit_exact(const std::vector<uint8_t>& img, const char* what) {
    rx8::Machine ref;
    ref.load(img);
    rx8::run(ref, 1000000);
    const std::string ref_dump = rx8::observable_dump(ref);


    const PipelineResult plain_r = run_pipeline(img, false, 1000000);
    EXPECT_TRUE(plain_r.halted && !plain_r.fault);
    EXPECT_EQ(plain_r.dump, ref_dump);

    const PipelineResult opt_r = run_pipeline(img, true, 1000000);
    EXPECT_TRUE(opt_r.halted && !opt_r.fault);
    EXPECT_EQ(opt_r.dump, ref_dump);  // optimized pipeline stays bit-exact
}

TEST(challenge, bench_bit_exact_through_full_pipeline) {
    expect_bit_exact(bench_image(), "bench");
}

TEST(challenge, smc_bit_exact_through_full_pipeline) {
    expect_bit_exact(smc_image(), "smc");
}

TEST(challenge, countdown_sum_bit_exact_and_faster) {
    auto img = to_image({
        rx8::enc(rx8::OP_ADDI, 1, 0, 0, 10),         // n = 10
        rx8::enc(rx8::OP_ADDI, 2, 0, 0, 0),          // sum = 0
        // loop @ 0x08
        rx8::enc(rx8::OP_ADD, 2, 2, 1),              // sum += n
        rx8::enc(rx8::OP_MOV, 3, 2),                 // dead copy bait
        rx8::enc(rx8::OP_ADDI, 1, 1, 0, -1 & 0xFFF), // n -= 1
        rx8::enc(rx8::OP_BNEZ, 0, 1, 0, 0x08 / 4),
        rx8::enc(rx8::OP_SW, 0, 2, 0, 1024),         // mem[1024] = sum
        rx8::enc(rx8::OP_OUT, 2),                    // 55
        rx8::enc(rx8::OP_HALT),
    });
    expect_bit_exact(img, "countdown");

    // Threshold gate on the benchmark-shaped workload: >= 20% fewer ops.
    const PipelineResult base = run_pipeline(img, false, 1000000);
    const PipelineResult opt = run_pipeline(img, true, 1000000);
    EXPECT_TRUE(opt.ops * 5 <= base.ops * 4);
}

}  // namespace
