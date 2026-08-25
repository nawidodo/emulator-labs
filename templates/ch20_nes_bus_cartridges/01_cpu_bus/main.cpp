#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>

#include "cpu.hpp"

using namespace nesbus;

namespace {

struct Rig {
    NesBus bus;
};

}  // namespace

TEST(mirrors, ram_window_mirrors_every_2k) {
    Rig r;
    r.bus.write(0x0000, 0xAB);
    EXPECT_EQ(r.bus.read(0x0000), 0xAB);
    EXPECT_EQ(r.bus.read(0x0800), 0xAB);
    EXPECT_EQ(r.bus.read(0x1000), 0xAB);
    EXPECT_EQ(r.bus.read(0x1800), 0xAB);

    r.bus.write(0x07FF, 0x5A);         // top of the window too
    EXPECT_EQ(r.bus.read(0x1FFF), 0x5A);

    // A write through a mirror lands in the one true RAM.
    r.bus.write(0x1234, 0x42);         // $1234 & $07FF = $234
    EXPECT_EQ(r.bus.read(0x0234), 0x42);
    EXPECT_EQ(r.bus.read(0x1234), 0x42);
    EXPECT_EQ(r.bus.read(0x1A34), 0x42);
}

TEST(mirrors, ram_is_only_2k_not_8k) {
    Rig r;
    // If the bus decoded $0000-$1FFF linearly, these would be distinct.
    r.bus.write(0x0100, 1);
    r.bus.write(0x0900, 2);            // same physical cell as $0100
    EXPECT_EQ(r.bus.read(0x0100), 2);
}

TEST(ppudec, registers_mirror_every_eight_bytes) {
    Rig r;
    r.bus.write(0x2000, 0x90);         // PPUCTRL
    EXPECT_EQ(r.bus.ppu.r[0], 0x90);
    EXPECT_EQ(r.bus.read(0x2008), 0x90);   // next mirror block
    EXPECT_EQ(r.bus.read(0x3000), 0x90);   // middle of the range
    EXPECT_EQ(r.bus.read(0x3FF8), 0x90);   // last mirrored register

    r.bus.write(0x2007, 0x77);         // PPUDATA through a mirror at $3FFF
    r.bus.write(0x3FFF, 0xEE);
    EXPECT_EQ(r.bus.ppu.r[7], 0xEE);
}

TEST(ppudec, all_eight_registers_are_distinct) {
    Rig r;
    for (uint16_t i = 0; i < 8; ++i) r.bus.write(static_cast<uint16_t>(0x2000 + i), static_cast<uint8_t>(i * 0x11));
    for (uint16_t i = 0; i < 8; ++i)
        EXPECT_EQ(r.bus.ppu.r[i], static_cast<uint8_t>(i * 0x11));
    // The stub decode aliases reg 2/3 into 0/1 — this test catches it.
    EXPECT_EQ(r.bus.read(0x2002), 0x22);
    EXPECT_EQ(r.bus.read(0x2003), 0x33);
}

TEST(controllers, strobe_then_shift_out_all_eight_buttons) {
    Rig r;
    // Press Start|B|A (read order: A, B, ... Start ...).
    r.bus.controller[0].set_buttons(
        Controller::BtnA | Controller::BtnB | Controller::Start);
    r.bus.write(0x4016, 0x01);         // strobe high: latch continuously
    r.bus.write(0x4016, 0x00);         // falling edge: snapshot frozen

    const uint8_t a      = r.bus.read(0x4016) & 0x01;
    const uint8_t b      = r.bus.read(0x4016) & 0x01;
    const uint8_t sel    = r.bus.read(0x4016) & 0x01;
    const uint8_t start  = r.bus.read(0x4016) & 0x01;
    const uint8_t up     = r.bus.read(0x4016) & 0x01;
    const uint8_t down   = r.bus.read(0x4016) & 0x01;
    const uint8_t left   = r.bus.read(0x4016) & 0x01;
    const uint8_t right  = r.bus.read(0x4016) & 0x01;

    EXPECT_EQ(a, 0x01);
    EXPECT_EQ(b, 0x01);
    EXPECT_EQ(sel, 0x00);
    EXPECT_EQ(start, 0x01);
    EXPECT_EQ(up, 0x00);
    EXPECT_EQ(down, 0x00);
    EXPECT_EQ(left, 0x00);
    EXPECT_EQ(right, 0x00);
}

TEST(controllers, reads_past_bit_eight_return_one) {
    Rig r;
    r.bus.controller[1].set_buttons(0x00);   // nothing pressed
    r.bus.write(0x4017, 0x01);
    r.bus.write(0x4017, 0x00);
    for (int i = 0; i < 8; ++i)
        EXPECT_EQ(r.bus.read(0x4017) & 0x01, 0x00);   // eight button bits
    for (int i = 0; i < 8; ++i)
        EXPECT_TRUE((r.bus.read(0x4017) & 0x01) != 0);  // ...then 1s forever
}

TEST(controllers, held_strobe_keeps_reporting_button_a) {
    Rig r;
    r.bus.controller[0].set_buttons(Controller::BtnA);
    r.bus.write(0x4016, 0x01);         // strobe held HIGH
    for (int i = 0; i < 4; ++i)
        EXPECT_EQ(r.bus.read(0x4016) & 0x01, 0x01);   // always A, no shift

    r.bus.controller[0].set_buttons(0x00);   // release A WHILE strobed:
    EXPECT_EQ(r.bus.read(0x4016) & 0x01, 0x00);   // snapshot follows live
}

TEST(dma, copies_256_bytes_from_cpu_page_into_oam) {
    Rig r;
    for (int i = 0; i < 256; ++i)
        r.bus.write(static_cast<uint16_t>(0x0200 + i),
                    static_cast<uint8_t>(i ^ 0x5A));
    r.bus.cycles |= 1;                 // deterministic ODD start cycle
    const uint64_t t0 = r.bus.cycles;
    r.bus.write(0x4014, 0x02);         // DMA from page $02
    for (int i = 0; i < 256; ++i)
        EXPECT_EQ(r.bus.oam[static_cast<size_t>(i)], uint8_t(i ^ 0x5A));
    // Odd start: exactly 513 debited from operation start.
    EXPECT_EQ(r.bus.cycles - t0, 514u);
}

TEST(dma, even_cycle_start_costs_the_extra_get_cycle) {
    Rig r;
    const uint64_t t0 = r.bus.cycles;  // starts at 0: the $4014 write
    r.bus.write(0x4014, 0x07);         // makes the copy begin on an ODD
    EXPECT_EQ(r.bus.cycles - t0, 515u);  // count -> +1 alignment cycle
}

TEST(dma, dma_reads_travel_through_the_full_decode) {
    Rig r;
    // Page $08 mirrors RAM page $00 ($0800 & $07FF): the DMA source read
    // must honor the mirror rules, proving it goes through the bus.
    r.bus.write(0x0010, 0xC7);
    r.bus.write(0x4014, 0x08);
    EXPECT_EQ(r.bus.oam[0x10], 0xC7);
}
