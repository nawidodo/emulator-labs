#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <vector>

#include "rx8.hpp"

namespace {

// The bench workload of tests/public/ch37_performance_dynarec/programs/
// bench.bin, built in-source so unit tests are self-contained.
std::vector<uint8_t> bench_image() {
    const uint32_t words[] = {
        rx8::enc(rx8::OP_ADDI, 7, 0, 0, 12),   // r7 = 12
        rx8::enc(rx8::OP_ADDI, 6, 0, 0, 3),    // r6 = 3
        rx8::enc(rx8::OP_ADDI, 1, 0, 0, 0),    // r1 = 0
        // loop @ 0x0c:
        rx8::enc(rx8::OP_ADD, 1, 1, 6),        // r1 += r6
        rx8::enc(rx8::OP_ADD, 3, 1, 0),        // r3 = r1 (copy bait)
        rx8::enc(rx8::OP_MOV, 4, 3),           // r4 = r3 (bait)
        rx8::enc(rx8::OP_ADD, 5, 4, 1),        // r5 = r4 + r1
        rx8::enc(rx8::OP_SW, 0, 5, 0, 512),    // mem[512] = r5
        rx8::enc(rx8::OP_ADDI, 2, 0, 0, 1),    // r2 = 1 (dead bait)
        rx8::enc(rx8::OP_ADDI, 7, 7, 0, -2 & 0xFFF),  // r7 -= 2
        rx8::enc(rx8::OP_BNEZ, 0, 7, 0, 0x0c / 4),    // bnez loop
        rx8::enc(rx8::OP_OUT, 1),
        rx8::enc(rx8::OP_HALT),
    };
    std::vector<uint8_t> img;
    for (uint32_t w : words)
        for (int b = 0; b < 4; ++b) img.push_back(uint8_t(w >> (8 * b)));
    return img;
}

}  // namespace

TEST(rx8, decode_field_extraction) {
    const rx8::Decoded d =
        rx8::decode(rx8::enc(0x0B, 3, 5, 2, 0xABC));
    EXPECT_EQ(d.op, uint8_t(0x0B));
    EXPECT_EQ(d.rd, uint8_t(3));
    EXPECT_EQ(d.rs, uint8_t(5));
    EXPECT_EQ(d.rt, uint8_t(2));
    EXPECT_EQ(d.imm12, uint16_t(0xABC));
}

TEST(rx8, immediates_sign_extend_targets_do_not) {
    const rx8::Decoded neg = rx8::decode(rx8::enc(rx8::OP_ADDI, 1, 2, 0, 0xFFE));
    EXPECT_EQ(neg.simm(), -2);
    const rx8::Decoded pos = rx8::decode(rx8::enc(rx8::OP_ADDI, 1, 2, 0, 0x7FF));
    EXPECT_EQ(pos.simm(), 2047);
    const rx8::Decoded br = rx8::decode(rx8::enc(rx8::OP_BNEZ, 0, 7, 0, 3));
    EXPECT_EQ(br.target(), uint32_t{12});  // absolute byte address
}

TEST(rx8, memory_alignment_and_range_faults) {
    rx8::Machine m;
    m.store_word(8, 0xDEADBEEF);
    EXPECT_TRUE(!m.fault);
    EXPECT_EQ(m.load_word(8), uint32_t{0xDEADBEEF});
    m.load_word(6);  // misaligned
    EXPECT_TRUE(m.fault);
    m.fault = false;
    m.store_word(rx8::kMemSize - 2, 1);  // runs off the end
    EXPECT_TRUE(m.fault);
}

TEST(rx8, alu_and_move_semantics) {
    rx8::Machine m;
    m.r[2] = 10;
    m.r[3] = 3;
    rx8::execute(m, rx8::decode(rx8::enc(rx8::OP_ADD, 1, 2, 3)));
    EXPECT_EQ(m.r[1], uint32_t{13});
    rx8::execute(m, rx8::decode(rx8::enc(rx8::OP_SUB, 4, 3, 2)));
    EXPECT_EQ(m.r[4], uint32_t{4294967289u});  // wraps: 3 - 10
    rx8::execute(m, rx8::decode(rx8::enc(rx8::OP_SHL, 5, 2, 3)));  // 10<<3
    EXPECT_EQ(m.r[5], uint32_t{80});
    rx8::execute(m, rx8::decode(rx8::enc(rx8::OP_SHR, 5, 5, 3)));
    EXPECT_EQ(m.r[5], uint32_t{10});
    rx8::execute(m, rx8::decode(rx8::enc(rx8::OP_XOR, 6, 2, 3)));
    EXPECT_EQ(m.r[6], uint32_t{9});
}

TEST(rx8, r0_is_hardwired_zero) {
    rx8::Machine m;
    m.r[1] = 55;
    rx8::execute(m, rx8::decode(rx8::enc(rx8::OP_ADDI, 0, 1, 0, 1)));
    EXPECT_EQ(m.r[0], uint32_t{0});
    m.mem[64] = 1;  // nonzero word at 64? word value 1
    rx8::execute(m, rx8::decode(rx8::enc(rx8::OP_LW, 0, 0, 0, 64)));
    EXPECT_EQ(m.r[0], uint32_t{0});
}

TEST(rx8, load_store_offsets_are_signed) {
    rx8::Machine m;
    m.r[1] = 200;
    // sw r3, -4(r1): fields carry rd=base(r1), rs=src(r3), imm12=-4.
    m.r[3] = 7;
    rx8::execute(m, rx8::decode(rx8::enc(rx8::OP_SW, 1, 3, 0, 0xFFC)));
    EXPECT_TRUE(!m.fault);
    EXPECT_EQ(m.read_le(196), uint32_t{7});
    // lw r4, -4(r1) reads it back through the same signed offset.
    rx8::execute(m, rx8::decode(rx8::enc(rx8::OP_LW, 4, 1, 0, 0xFFC)));
    EXPECT_EQ(m.r[4], uint32_t{7});
}

TEST(rx8, branches_use_absolute_targets) {
    rx8::Machine m;
    m.pc = 0x20;
    m.r[4] = 0;
    const uint32_t before = m.pc + 4;
    rx8::execute(m, rx8::decode(rx8::enc(rx8::OP_BEQZ, 0, 4, 0, 0x40 / 4)));
    EXPECT_EQ(m.pc, uint32_t{0x40});
    m.pc = before;  // restore the default advance for the not-taken case
    m.r[4] = 1;
    rx8::execute(m, rx8::decode(rx8::enc(rx8::OP_BEQZ, 0, 4, 0, 0x40 / 4)));
    EXPECT_EQ(m.pc, before);
    rx8::execute(m, rx8::decode(rx8::enc(rx8::OP_JMP, 0, 0, 0, 0x100 / 4)));
    EXPECT_EQ(m.pc, uint32_t{0x100});
}

TEST(rx8, out_appends_halt_stops_unknown_faults) {
    rx8::Machine m;
    m.r[7] = 9;
    rx8::execute(m, rx8::decode(rx8::enc(rx8::OP_OUT, 7)));
    EXPECT_EQ(m.out.size(), size_t{1});
    if (m.out.size() == 1) EXPECT_EQ(m.out[0], uint32_t{9});
    rx8::execute(m, rx8::decode(rx8::enc(rx8::OP_HALT)));
    EXPECT_TRUE(m.halted);
    EXPECT_EQ(m.step(), 0);  // sticky halt refuses further work
    rx8::Machine bad;
    rx8::execute(bad, rx8::decode(rx8::enc(0x77)));
    EXPECT_TRUE(bad.fault);
}

TEST(rx8, bench_executed_instruction_count) {
    rx8::Machine m;
    auto img = bench_image();
    m.load(img);
    const uint64_t n = rx8::run(m, 10000);
    // 3 setup + 6 iterations x 8 body instructions + OUT + HALT.
    EXPECT_EQ(n, uint64_t{53});
    EXPECT_EQ(m.executed, uint64_t{53});
    EXPECT_TRUE(m.halted);
    EXPECT_TRUE(!m.fault);
    EXPECT_EQ(m.out.size(), size_t{1});
    if (m.out.size() == 1) EXPECT_EQ(m.out[0], uint32_t{18});  // 6 iters * 3
    EXPECT_EQ(m.read_le(512), uint32_t{36});
}

TEST(rx8, observable_dump_is_canonical) {
    rx8::Machine m;
    auto img = bench_image();
    m.load(img);
    rx8::run(m, 10000);
    const std::string dump = rx8::observable_dump(m);
    EXPECT_TRUE(dump.find("out 00000012\n") != std::string::npos);
    EXPECT_TRUE(dump.find("mem 0200=00000024\n") != std::string::npos);
}
