#define LABSTEST_MAIN
#include "labstest.hpp"

#include "cpu.hpp"

using namespace nes6502;

namespace {

struct Rig {
    FlatRam ram;
    Cpu cpu{.bus = &ram};

    void exec(std::initializer_list<uint8_t> bytes) {
        cpu.load_program(0x0200, bytes.begin(), bytes.size());
        step(cpu);
    }
};

}  // namespace

// Regression tests for the two seeded bugs. These run RED until both bugs
// are fixed; keep them green afterwards (add them to your bug-report.md).

TEST(regression, zpx_stays_inside_page_zero) {
    Rig r;
    r.cpu.load_program(0x0200, nullptr, 0);
    r.ram.mem[0x0200] = 0x80;  // operand byte
    r.cpu.x = 0x90;
    uint16_t addr = 0;
    (void)nes6502::mode_zpx(r.cpu, addr);
    EXPECT_EQ(addr, 0x0010);   // bug reads $0110 here
}

TEST(regression, sta_zpx_writes_page_zero_after_wrap) {
    Rig r;
    r.ram.mem[0x0010] = 0x99;
    r.exec({0xB5, 0x10});     // LDA $10,X (X=$00)
    EXPECT_EQ(r.cpu.a, 0x99);
    r.cpu.x = 0xF0;
    r.ram.mem[0x0082] = 0x77; // $92 + $F0 wraps to $82
    r.ram.mem[0x0182] = 0xEE; // the buggy build writes HERE instead
    r.cpu.load_program(0x0200, nullptr, 0);
    r.cpu.pc = 0x0200;
    r.ram.mem[0x0200] = 0x95;  // STA $92,X
    r.ram.mem[0x0201] = 0x92;
    r.cpu.a = 0x42;
    step(r.cpu);
    EXPECT_EQ(r.ram.mem[0x0082], 0x42);
    EXPECT_EQ(r.ram.mem[0x0182], 0xEE);
}

TEST(regression, izy_bills_page_cross_penalty) {
    Rig r;
    r.ram.mem[0x40] = 0xFF;
    r.ram.mem[0x41] = 0x20;   // base $20FF
    r.cpu.y = 0x01;           // -> $2100: crossed
    r.exec({0xB1, 0x40});     // LDA ($40),Y
    EXPECT_EQ(r.cpu.cycles, 6);  // buggy build reports 5

    Rig r2;
    r2.ram.mem[0x40] = 0x00;
    r2.ram.mem[0x41] = 0x20;  // base $2000, Y=1 -> no cross
    r2.cpu.y = 0x01;
    r2.exec({0xB1, 0x40});
    EXPECT_EQ(r2.cpu.cycles, 5);
}
