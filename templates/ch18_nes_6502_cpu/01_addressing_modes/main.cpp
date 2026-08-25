#define LABSTEST_MAIN
#include "labstest.hpp"

#include "cpu.hpp"

using nes6502::Cpu;
using nes6502::FlatRam;

namespace {

struct Rig {
    FlatRam ram;
    Cpu cpu{.bus = &ram};
    void program(std::initializer_list<uint8_t> bytes) {
        cpu.load_program(0x0200, bytes.begin(), bytes.size());
    }
};

constexpr bool kNoCross = false;
constexpr bool kCross = true;

bool run_mode(bool (*fn)(Cpu&, uint16_t&), Cpu& cpu, uint16_t& addr) {
    return fn(cpu, addr);
}

}  // namespace

TEST(modes, imm_consumes_one_byte) {
    Rig r;
    r.program({0xEA});
    uint16_t addr = 0;
    const bool crossed = run_mode(nes6502::mode_imm, r.cpu, addr);
    EXPECT_EQ(crossed, kNoCross);
    EXPECT_EQ(addr, 0x0200);
    EXPECT_EQ(r.cpu.pc, 0x0201);
    EXPECT_EQ(r.cpu.cycles, 0);  // operand fetch not billed by the mode
}

TEST(modes, zp_reads_operand) {
    Rig r;
    r.program({0x80});
    uint16_t addr = 0;
    const bool crossed = run_mode(nes6502::mode_zp, r.cpu, addr);
    EXPECT_EQ(crossed, kNoCross);
    EXPECT_EQ(addr, 0x0080);
    EXPECT_EQ(r.cpu.cycles, 1);
}

TEST(modes, zpx_wraps_in_page_zero) {
    Rig r;
    r.program({0x80});
    r.cpu.x = 0x90;
    uint16_t addr = 0;
    const bool crossed = run_mode(nes6502::mode_zpx, r.cpu, addr);
    EXPECT_EQ(crossed, kNoCross);
    EXPECT_EQ(addr, 0x0010);  // $80 + $90 wraps to $10, never $110
}

TEST(modes, zpy_uses_y_index) {
    Rig r;
    r.program({0x40});
    r.cpu.y = 0x30;
    uint16_t addr = 0;
    const bool crossed = run_mode(nes6502::mode_zpy, r.cpu, addr);
    EXPECT_EQ(crossed, kNoCross);
    EXPECT_EQ(addr, 0x0070);
}

TEST(modes, abs_little_endian_fetch) {
    Rig r;
    r.program({0x34, 0x12});
    uint16_t addr = 0;
    const bool crossed = run_mode(nes6502::mode_abs, r.cpu, addr);
    EXPECT_EQ(crossed, kNoCross);
    EXPECT_EQ(addr, 0x1234);
    EXPECT_EQ(r.cpu.cycles, 2);
}

TEST(modes, absx_no_cross_no_penalty_reported) {
    Rig r;
    r.program({0x00, 0x20});
    r.cpu.x = 0x05;
    uint16_t addr = 0;
    const bool crossed = run_mode(nes6502::mode_absx, r.cpu, addr);
    EXPECT_EQ(crossed, kNoCross);
    EXPECT_EQ(addr, 0x2005);
}

TEST(modes, absx_page_cross_reported) {
    Rig r;
    r.program({0xFF, 0x20});  // base $20FF
    r.cpu.x = 0x02;           // -> $2101 crosses into page $21
    uint16_t addr = 0;
    const bool crossed = run_mode(nes6502::mode_absx, r.cpu, addr);
    EXPECT_EQ(crossed, kCross);
    EXPECT_EQ(addr, 0x2101);
}

TEST(modes, absy_page_boundary_exactly_is_not_a_cross) {
    Rig r;
    r.program({0x00, 0x21});
    r.cpu.y = 0x00;
    uint16_t addr = 0;
    const bool crossed = run_mode(nes6502::mode_absy, r.cpu, addr);
    EXPECT_EQ(crossed, kNoCross);
    EXPECT_EQ(addr, 0x2100);
}

TEST(modes, izy_pointer_and_cross) {
    Rig r;
    r.program({0x40});
    r.ram.mem[0x40] = 0xFF;
    r.ram.mem[0x41] = 0x20;   // base $20FF
    r.cpu.y = 0x01;           // -> $2100: crossed
    uint16_t addr = 0;
    const bool crossed = run_mode(nes6502::mode_izy, r.cpu, addr);
    EXPECT_EQ(crossed, kCross);
    EXPECT_EQ(addr, 0x2100);
    EXPECT_EQ(r.cpu.cycles, 3);  // operand + two pointer reads
}

TEST(modes, izx_pointer_bytes_wrap_in_page_zero) {
    Rig r;
    r.program({0xFE});
    r.cpu.x = 0x03;           // pointer $01 (wrapped past $FF)
    r.ram.mem[0x01] = 0x34;
    r.ram.mem[0x02] = 0x12;
    uint16_t addr = 0;
    const bool crossed = run_mode(nes6502::mode_izx, r.cpu, addr);
    EXPECT_EQ(crossed, kNoCross);
    EXPECT_EQ(addr, 0x1234);
}

TEST(modes, izy_pointer_high_byte_wraps_in_page_zero) {
    Rig r;
    r.program({0xFF});
    r.ram.mem[0xFF] = 0x78;   // lo from $00FF
    r.ram.mem[0x00] = 0x56;   // hi from $0000 (wrapped!), not $0100
    r.cpu.y = 0x00;
    uint16_t addr = 0;
    const bool crossed = run_mode(nes6502::mode_izy, r.cpu, addr);
    EXPECT_EQ(crossed, kNoCross);
    EXPECT_EQ(addr, 0x5678);
}

TEST(modes, ind_jmp_pointer_page_wrap_quirk) {
    Rig r;
    r.program({0xFF, 0x30});  // pointer at $30FF
    r.ram.mem[0x30FF] = 0x11;
    r.ram.mem[0x3000] = 0x22;  // hi byte comes from $3000, not $3100
    uint16_t addr = 0;
    const bool crossed = run_mode(nes6502::mode_ind, r.cpu, addr);
    EXPECT_EQ(crossed, kNoCross);
    EXPECT_EQ(addr, 0x2211);
}
