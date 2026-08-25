#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include "decode_cache.hpp"

namespace {

// smc.bin of tests/public/ch37_performance_dynarec/programs/: patches its
// own OUT instruction at 0x08 after pass 1, then re-executes it.
std::vector<uint8_t> smc_image() {
    const uint32_t words[] = {
        rx8::enc(rx8::OP_ADDI, 4, 0, 0, 2),        // r4 = 2 passes
        rx8::enc(rx8::OP_ADDI, 2, 0, 0, 111),      // pass-1 output value
        rx8::enc(rx8::OP_OUT, 2),                  // site @ 0x08
        rx8::enc(rx8::OP_BEQZ, 0, 4, 0, 0x20 / 4),
        rx8::enc(rx8::OP_ADDI, 4, 4, 0, -1 & 0xFFF),
        rx8::enc(rx8::OP_ADDI, 3, 0, 0, 9),        // payload
        rx8::enc(rx8::OP_SW, 0, 3, 0, 8),          // mem[8] := ADDI r2,r0,9
        rx8::enc(rx8::OP_JMP, 0, 0, 0, 0x08 / 4),
        rx8::enc(rx8::OP_HALT),
    };
    std::vector<uint8_t> img;
    for (uint32_t w : words)
        for (int b = 0; b < 4; ++b) img.push_back(uint8_t(w >> (8 * b)));
    return img;
}

}  // namespace

TEST(cache, lookup_insert_and_invalidation_still_work) {
    rx8::DecodeCache c;
    EXPECT_TRUE(c.lookup(8) == nullptr);
    c.insert(8, rx8::decode(rx8::enc(rx8::OP_NOP)));
    EXPECT_TRUE(c.lookup(8) != nullptr);
    EXPECT_EQ(c.invalidate_range(8, 4), size_t{1});
    EXPECT_TRUE(c.lookup(8) == nullptr);
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
    // Exactly ONE out-entry: after pass 1 the site is an ADDI, not an OUT.
    EXPECT_EQ(cpu.m.out.size(), size_t{1});
    if (cpu.m.out.size() == 1) EXPECT_EQ(cpu.m.out[0], uint32_t{111});
    EXPECT_TRUE(cpu.stats.hits > 0);
}

TEST(smc, fixture_is_sensitive_to_a_non_invalidating_cache) {
    // Control experiment: simulate the bug by bypassing invalidation on a
    // KNOWN-good pipeline shape (direct decode caching without any store
    // hook). The fixture must notice — otherwise it proves nothing.
    auto img = smc_image();
    rx8::DecodeCache cache;
    rx8::Machine m;
    m.load(img);
    uint64_t n = 0;
    while (n < 10000 && !m.halted && !m.fault) {
        const uint32_t at = m.pc;
        m.pc += 4;
        const rx8::Decoded* d = cache.lookup(at);   // never invalidated...
        rx8::execute(m, d ? *d : [&] {
            const rx8::Decoded fresh = rx8::decode(m.load_word(at));
            cache.insert(at, fresh);
            return fresh;
        }());
        ++m.executed;
        ++n;
    }
    EXPECT_EQ(m.out.size(), size_t{3});  // stale OUT fires on every pass

    rx8::Machine ref;
    ref.load(img);
    rx8::run(ref, 10000);
    EXPECT_NE(rx8::observable_dump(m), rx8::observable_dump(ref));
}
