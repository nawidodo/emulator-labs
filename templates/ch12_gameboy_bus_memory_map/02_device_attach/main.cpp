// Tests for 02_device_attach: the full machine map routes traffic to
// eight distinct devices without cross-talk. Hidden grading filters
// suites by the "attach." prefix.
#define LABSTEST_MAIN
#include <cstdint>
#include <utility>

#include "labstest.hpp"
#include "machine.hpp"

using gbmachine::Machine;

namespace {

// One representative address per attached device (cart excluded — it is
// read-only). Each marker is unique so any misroute is visible.
constexpr std::pair<uint16_t, uint8_t> kProbes[] = {
    {0x8123, 0x21},  // VRAM
    {0xA345, 0x32},  // external RAM
    {0xC567, 0x43},  // WRAM
    {0xFE10, 0x54},  // OAM
    {0xFF26, 0x65},  // I/O registers
    {0xFF90, 0x76},  // HRAM
    {0xFFFF, 0x87},  // IE latch
};

}  // namespace

TEST(attach, cart_reads_come_from_the_committed_image) {
    auto img = gbmachine::makeTestCart();
    Machine m(img);
    EXPECT_EQ(m.bus.read(0x0000), img[0x0000]);
    EXPECT_EQ(m.bus.read(0x0100), img[0x0100]);  // entry point lands here on real hardware
    EXPECT_EQ(m.bus.read(0x4000), img[0x4000]);
    EXPECT_EQ(m.bus.read(0x7FFF), img[0x7FFF]);
}

TEST(attach, every_device_round_trips_through_the_bus) {
    auto img = gbmachine::makeTestCart();
    Machine m(img);
    for (const auto& [addr, mark] : kProbes) m.bus.write(addr, mark);
    for (const auto& [addr, mark] : kProbes) EXPECT_EQ(m.bus.read(addr), mark);
}

TEST(attach, regions_never_bleed_into_each_other) {
    auto img = gbmachine::makeTestCart();
    for (const auto& [addr, mark] : kProbes) {
        Machine m(img);  // fresh map per probe
        m.bus.write(addr, mark);
        for (const auto& [other, otherMark] : kProbes) {
            if (other == addr) continue;
            EXPECT_EQ(m.bus.read(other), 0x00);  // untouched devices still power-on $00
            (void)otherMark;
        }
    }
}

TEST(attach, cart_rom_silently_drops_writes) {
    auto img = gbmachine::makeTestCart();
    Machine m(img);
    m.bus.write(0x0123, 0x99);
    m.bus.write(0x7FFF, 0xEE);
    EXPECT_EQ(m.bus.read(0x0123), img[0x0123]);
    EXPECT_EQ(m.bus.read(0x7FFF), img[0x7FFF]);
}

TEST(attach, oam_ends_at_fe9f_then_documented_gap) {
    auto img = gbmachine::makeTestCart();
    Machine m(img);
    m.bus.write(0xFE9F, 0x5A);   // last real OAM byte: routes to the OAM device
    EXPECT_EQ(m.bus.read(0xFE9F), 0x5A);
    EXPECT_EQ(m.oam.cells()[0x9F], 0x5A);
    m.bus.write(0xFEA0, 0xA5);   // unusable page: dropped per policy
    m.bus.write(0xFEFF, 0xC3);
    EXPECT_EQ(m.bus.read(0xFEA0), 0x00);
    EXPECT_EQ(m.bus.read(0xFEFF), 0x00);
}

TEST(attach, hram_and_ie_meet_at_fffe_ffff) {
    auto img = gbmachine::makeTestCart();
    Machine m(img);
    m.bus.write(0xFFFE, 0xAA);   // last HRAM byte
    m.bus.write(0xFFFF, 0xBB);   // IE register: separate single-byte device
    EXPECT_EQ(m.bus.read(0xFFFE), 0xAA);
    EXPECT_EQ(m.bus.read(0xFFFF), 0xBB);
    EXPECT_NE(m.bus.read(0xFFFE), m.bus.read(0xFFFF));
}

TEST(attach, io_window_stops_before_hram) {
    auto img = gbmachine::makeTestCart();
    Machine m(img);
    m.bus.write(0xFF7F, 0x11);   // last I/O byte
    m.bus.write(0xFF80, 0x22);   // first HRAM byte
    EXPECT_EQ(m.bus.read(0xFF7F), 0x11);
    EXPECT_EQ(m.bus.read(0xFF80), 0x22);
    EXPECT_EQ(m.ioRegs.cells()[0x7F], 0x11);
    EXPECT_EQ(m.hram.cells()[0x00], 0x22);
}
