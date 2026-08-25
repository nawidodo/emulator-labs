#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include "decode_cache.hpp"

namespace {

// The smc fixture of tests/public/ch37_performance_dynarec/programs/
// smc.bin, built in-source: the OUT at 0x08 executes on pass 1, then the
// program patches that very word into ADDI r2,r0,9 and jumps back.
std::vector<uint8_t> smc_image() {
    const uint32_t words[] = {
        rx8::enc(rx8::OP_ADDI, 4, 0, 0, 2),    // r4 = 2 passes
        rx8::enc(rx8::OP_ADDI, 2, 0, 0, 111),  // r2 = 111 (pass-1 value)
        // site @ 0x08 (patched to ADDI r2,r0,9 by the store below):
        rx8::enc(rx8::OP_OUT, 2),
        rx8::enc(rx8::OP_BEQZ, 0, 4, 0, 0x20 / 4),  // all passes done?
        rx8::enc(rx8::OP_ADDI, 4, 4, 0, -1 & 0xFFF),
        rx8::enc(rx8::OP_ADDI, 3, 0, 0, 9),    // payload
        rx8::enc(rx8::OP_SW, 0, 3, 0, 8),      // mem[8] := patch word
        rx8::enc(rx8::OP_JMP, 0, 0, 0, 0x08 / 4),
        rx8::enc(rx8::OP_HALT),                // @ 0x20
    };
    std::vector<uint8_t> img;
    for (uint32_t w : words)
        for (int b = 0; b < 4; ++b) img.push_back(uint8_t(w >> (8 * b)));
    return img;
}

}  // namespace

TEST(cache, insert_then_lookup_hits) {
    rx8::DecodeCache c;
    EXPECT_TRUE(c.lookup(12) == nullptr);
    c.insert(12, rx8::decode(rx8::enc(rx8::OP_MOV, 1, 2)));
    const rx8::Decoded* d = c.lookup(12);
    EXPECT_TRUE(d != nullptr);
    if (d != nullptr) {
        EXPECT_EQ(d->op, uint8_t{rx8::OP_MOV});
        EXPECT_EQ(d->rd, uint8_t{1});
    }
}

TEST(cache, invalidate_drops_only_overlapping_entries) {
    rx8::DecodeCache c;
    c.insert(4, {});
    c.insert(8, {});
    c.insert(12, {});
    // [10,12) overlaps only the word at pc=8 ([8,12)).
    EXPECT_EQ(c.invalidate_range(10, 2), size_t{1});
    EXPECT_TRUE(c.lookup(4) != nullptr);
    EXPECT_TRUE(c.lookup(8) == nullptr);
    EXPECT_TRUE(c.lookup(12) != nullptr);
    EXPECT_EQ(c.size(), size_t{2});
}

TEST(cache, invalidate_exact_word) {
    rx8::DecodeCache c;
    for (uint32_t pc = 0; pc < 32; pc += 4) c.insert(pc, {});
    EXPECT_EQ(c.invalidate_range(16, 4), size_t{1});
    EXPECT_TRUE(c.lookup(16) == nullptr);
    EXPECT_EQ(c.size(), size_t{7});
    // A far-away store invalidates nothing.
    EXPECT_EQ(c.invalidate_range(4096, 4), size_t{0});
}

TEST(smc, cached_matches_switch_interpreter) {
    auto img = smc_image();

    rx8::Machine ref;
    ref.load(img);
    rx8::run(ref, 10000);

    rx8::CachedCpu cpu;
    cpu.m.load(img);
    uint64_t n = 0;
    while (n < 10000 && cpu.step() == 1) ++n;

    EXPECT_TRUE(cpu.m.halted);
    EXPECT_TRUE(!cpu.m.fault);
    EXPECT_EQ(rx8::observable_dump(cpu.m), rx8::observable_dump(ref));
    // pass 1 only; later passes see the patched-in ADDI instead of an OUT.
    EXPECT_EQ(cpu.m.out.size(), size_t{1});
    if (cpu.m.out.size() == 1) EXPECT_EQ(cpu.m.out[0], uint32_t{111});
    EXPECT_TRUE(cpu.stats.hits > 0);       // the loop really used the cache
    EXPECT_TRUE(cpu.stats.invalidations > 0);  // stores into code invalidated
}

TEST(smc, fixture_is_sensitive_to_stale_cache) {
    // Prove the fixture catches the bug class: a cache that never
    // invalidates keeps executing the OLD out-instruction and the log
    // grows to three entries instead of one.
    auto img = smc_image();
    rx8::CachedCpu stale;
    stale.auto_invalidate = false;
    stale.m.load(img);
    uint64_t n = 0;
    while (n < 10000 && stale.step() == 1) ++n;

    EXPECT_EQ(stale.m.out.size(), size_t{3});

    rx8::Machine ref;
    ref.load(img);
    rx8::run(ref, 10000);
    EXPECT_NE(rx8::observable_dump(stale.m), rx8::observable_dump(ref));
}
