#define LABSTEST_MAIN
#include <span>

#include "labstest.hpp"
#include "../03_ld_alu/bus.hpp"
#include "../03_ld_alu/cpu.hpp"
#include "fixtures.hpp"

namespace {

struct Result {
    uint16_t af, bc, de, hl, sp, pc;
    uint64_t cyc;
    bool halted;
};

Result run(std::span<const uint8_t> program) {
    gb::FlatBus bus;
    bus.load(program);
    gb::Cpu cpu;
    cpu.bus = &bus;
    for (int i = 0; i < 200000 && !cpu.halted && !cpu.trap; ++i) cpu.step();
    return {cpu.af(), cpu.bc(), cpu.de(), cpu.hl(), cpu.sp, cpu.pc,
            cpu.cyc, cpu.halted};
}

}  // namespace

TEST(challenge, sm01_alu_ops_golden) {
    const Result r = run(ch10_fixtures::sm01_alu_ops);
    EXPECT_TRUE(r.halted);
    EXPECT_EQ(r.af, 0x42C0);
    EXPECT_EQ(r.bc, 0x3C0F);
    EXPECT_EQ(r.cyc, 236);
}

TEST(challenge, sm01_result_memory_golden) {
    gb::FlatBus bus;
    bus.load(ch10_fixtures::sm01_alu_ops);
    gb::Cpu cpu;
    cpu.bus = &bus;
    for (int i = 0; i < 200000 && !cpu.halted && !cpu.trap; ++i) cpu.step();
    const uint8_t expect[] = {0x4B, 0x00, 0x20, 0x2F, 0x00, 0x00, 0x55, 0x36};
    for (int i = 0; i < 8; ++i)
        EXPECT_EQ(bus.mem[0xC100 + i], expect[i]);
}

TEST(challenge, sm02_loads_loop_golden) {
    const Result r = run(ch10_fixtures::sm02_loads_loop);
    EXPECT_TRUE(r.halted);
    EXPECT_EQ(r.af, 0x0380);
    EXPECT_EQ(r.hl, 0xC244);
    EXPECT_EQ(r.bc, 0x0234);
    EXPECT_EQ(r.cyc, 204);
}

TEST(challenge, sm02_fill_pattern_golden) {
    gb::FlatBus bus;
    bus.load(ch10_fixtures::sm02_loads_loop);
    gb::Cpu cpu;
    cpu.bus = &bus;
    for (int i = 0; i < 200000 && !cpu.halted && !cpu.trap; ++i) cpu.step();
    EXPECT_EQ(bus.mem[0xC010], 3);
    EXPECT_EQ(bus.mem[0xC011], 0);  // ldd wrote A=0 over the stored 2
    EXPECT_EQ(bus.mem[0xC012], 1);
    EXPECT_EQ(bus.mem[0xC020], 3);
}

TEST(challenge, sm03_cond_loops_golden) {
    const Result r = run(ch10_fixtures::sm03_cond_loops);
    EXPECT_TRUE(r.halted);
    EXPECT_EQ(r.af, 0x0CC0);  // A=12 iterations, Z from dec b hitting 0
    EXPECT_EQ(r.cyc, 324);
}
