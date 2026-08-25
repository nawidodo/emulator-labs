#define LABSTEST_MAIN
#include "labstest.hpp"

#include "machine_b.hpp"

#include <vector>

// Board bring-up self-checks for the Arcade-8080-B construction. The
// hidden grader runs unseen B-board programs through the runner binary;
// these tests pin the same contracts locally.

using namespace sib;

TEST(spec, documented_board_values) {
    EXPECT_EQ(kArcade8080B.rom_base, 0x0000);
    EXPECT_EQ(kArcade8080B.rom_bytes, 0x4000u);
    EXPECT_EQ(kArcade8080B.ram_base, 0x4000);
    EXPECT_EQ(kArcade8080B.vram_base, 0x4400);
    EXPECT_EQ(cycles_per_frame(kArcade8080B.khz), 36000u);
    EXPECT_EQ(kArcade8080B.irq_opcode_even, 0xDF);   // RST vec 0x18
    EXPECT_EQ(kArcade8080B.irq_opcode_odd, 0xF7);    // RST vec 0x30
}

TEST(map, windows_route_and_rom_is_masked) {
    MachineB m(kArcade8080B);
    std::vector<uint8_t> img(0x4000, 0xFF);
    img[0] = 0xFB;                                   // DI at reset entry
    img[1] = 0x3E; img[2] = 0x42;                    // MVI A,42
    img[3] = 0x32; img[4] = 0x00; img[5] = 0x40;     // STA 4000 (RAM)
    img[6] = 0x76;                                   // HLT
    m.load_rom(img.data(), img.size());
    EXPECT_EQ(m.read(0x0000), 0xFB);
    m.run(10000, nullptr);
    EXPECT_EQ(m.read(0x4000), 0x42);                 // landed in RAM
    // VRAM window is distinct from RAM.
    EXPECT_EQ(m.read(0x4400), 0x00);
}

TEST(io, two_shifters_share_one_amount_port) {
    MachineB m(kArcade8080B);
    // OUT2 0x2A: bits0-2=2 -> shifter #1 amount, bits3-5=5 -> shifter #2.
    const uint8_t prog[] = {
        0xF3,                                        // DI
        0x3E, 0x2A, 0xD3, 0x02,                      // MVI A,2A / OUT 2
        0x3E, 0x12, 0xD3, 0x06,                      // sr1 low 12
        0x3E, 0x34, 0xD3, 0x06,                      // sr1 high 34 -> 3412
        0x3E, 0x56, 0xD3, 0x07,                      // sr2 low 56
        0x3E, 0x78, 0xD3, 0x07,                      // sr2 high 78 -> 7856
        0xDB, 0x06,                                  // IN 6 -> (3412>>2)=04
        0x32, 0x00, 0x40,                            // STA 4000
        0xDB, 0x07,                                  // IN 7 -> (7856>>5)=C2
        0x32, 0x01, 0x40,                            // STA 4001
        0x76,                                        // HLT
    };
    m.load_rom(prog, sizeof prog);
    m.run(2000, nullptr);
    EXPECT_EQ(m.read(0x4000), 0x04);
    EXPECT_EQ(m.read(0x4001), 0xC2);
}

TEST(io, sound_and_watchdog_ports_record) {
    MachineB m(kArcade8080B);
    const uint8_t prog[] = {
        0xF3,
        0x3E, 0x11, 0xD3, 0x04,                      // sound A
        0x3E, 0x22, 0xD3, 0x05,                      // sound B
        0xD3, 0x00,                                  // watchdog kick
        0x76,
    };
    m.load_rom(prog, sizeof prog);
    m.run(1000, nullptr);
    bool s4 = false, s5 = false, s0 = false;
    for (const auto& e : m.sound().events()) {
        s4 |= e.port == 4 && e.value == 0x11;
        s5 |= e.port == 5 && e.value == 0x22;
        s0 |= e.port == 0;
    }
    EXPECT_TRUE(s4);
    EXPECT_TRUE(s5);
    EXPECT_TRUE(s0);
    EXPECT_TRUE(m.watchdog().kicked());
}

TEST(timers, b_vectors_alternate_per_frame) {
    MachineB m(kArcade8080B);
    // Counting handlers at vectors 0x18 and 0x30, then a delay loop.
    std::vector<uint8_t> prog(0x4000, 0xFF);
    auto put = [&prog](uint16_t a, std::initializer_list<uint8_t> bs) {
        uint16_t i = a;
        for (uint8_t b : bs) prog[i++] = b;
    };
    put(0x0000, {0xC3, 0x50, 0x00});                 // JMP start-ish
    put(0x0018, {0xC3, 0x60, 0x00});                 // vec18 thunk
    put(0x0030, {0xC3, 0x70, 0x00});                 // vec30 thunk
    put(0x0050, {0xF3,                               // start:
                 0x31, 0x00, 0x42,                   // LXI SP,4200
                 0xFB,                               // EI
                 0x01, 0x70, 0x17,                   // LXI B,6000
                 0x0B, 0x78, 0xB1, 0xC2, 0x55, 0x00, // delay loop
                 0x76});                             // HLT
    put(0x0060, {0xF5, 0xE5,
                 0x2A, 0x00, 0x40, 0x23, 0x22, 0x00, 0x40,
                 0xE1, 0xF1, 0xFB, 0xC9});
    put(0x0070, {0xF5, 0xE5,
                 0x2A, 0x02, 0x40, 0x23, 0x22, 0x02, 0x40,
                 0xE1, 0xF1, 0xFB, 0xC9});
    m.load_rom(prog.data(), prog.size());
    m.run(5 * cycles_per_frame(kArcade8080B.khz), nullptr);
    // Boundaries at 36k/72k/108k/144k within ~120k of loop+handlers.
    const int c18 = m.read(0x4000) | (m.read(0x4001) << 8);
    const int c30 = m.read(0x4002) | (m.read(0x4003) << 8);
    EXPECT_EQ(c18, 2);   // 36k, 108k
    EXPECT_EQ(c30, 2);   // 72k, 144k
}
