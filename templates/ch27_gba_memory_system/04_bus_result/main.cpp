#define LABSTEST_MAIN
#include "labstest.hpp"
#include <memory>
#include "gba_bus.hpp"

using namespace gba;

TEST(bus, bus_result_shape_is_value_and_cycles) {
    // The curriculum-mandated interface.
    BusResult r{42, 1};
    EXPECT_EQ(r.value, 42u);
    EXPECT_EQ(r.cycles, 1u);
}

TEST(bus, ewram_write_read_roundtrip_with_cost) {
    auto bus_storage = std::make_unique<Bus>();
    Bus& bus = *bus_storage;
    bus.write(0x02000000, 4, 0xDEADBEEFu);
    const BusResult r = bus.read(0x02000000, 4);
    EXPECT_EQ(r.value, 0xDEADBEEFu);
    EXPECT_EQ(r.cycles, 3u);              // non-sequential EWRAM
}

TEST(bus, iwram_runs_at_one_cycle) {
    auto bus_storage = std::make_unique<Bus>();
    Bus& bus = *bus_storage;
    bus.write(0x03000010, 2, 0x1234);
    const BusResult r = bus.read(0x03000010, 2);
    EXPECT_EQ(r.value, 0x1234u);
    EXPECT_EQ(r.cycles, 1u);
}

TEST(bus, rom_chips_report_their_waitstates) {
    auto b0_storage = std::make_unique<Bus>();
    Bus& b0 = *b0_storage;
    b0.rom0[0] = 0x42;
    const BusResult r0 = b0.read(0x08000000, 2);
    EXPECT_EQ(r0.value, 0x0042u);
    EXPECT_EQ(r0.cycles, 4u);             // WS0 N

    auto b1_storage = std::make_unique<Bus>();
    Bus& b1 = *b1_storage;
    b1.rom1[0x100] = 0x99;
    const BusResult r1 = b1.read(0x0A000100, 2);
    EXPECT_EQ(r1.value, 0x99u);           // little-endian halfword
    EXPECT_EQ(r1.cycles, 3u);             // WS1 N

    auto b2_storage = std::make_unique<Bus>();
    Bus& b2 = *b2_storage;
    const BusResult r2 = b2.read(0x0C000002, 2);
    EXPECT_EQ(r2.cycles, 5u);             // WS2 N
}

TEST(bus, unaligned_word_read_rotates) {
    auto bus_storage = std::make_unique<Bus>();
    Bus& bus = *bus_storage;
    bus.write(0x02000100, 4, 0x11223344u);
    const BusResult r = bus.read(0x02000102, 4);
    EXPECT_EQ(r.value, 0x33441122u);      // rotated right by 16 bits
}

TEST(bus, vram_discontinuity_visible_through_bus) {
    auto bus_storage = std::make_unique<Bus>();
    Bus& bus = *bus_storage;
    bus.write(0x06004444, 2, 0x1111);     // BG half
    bus.write(0x06014444, 2, 0x2222);     // OBJ half: NOT aliased to BG
    EXPECT_EQ(bus.read(0x06004444, 2).value, 0x1111u);
    EXPECT_EQ(bus.read(0x06014444, 2).value, 0x2222u);
    // Top hole reflects the BG upper half.
    bus.write(0x06008888, 2, 0xABCD);
    EXPECT_EQ(bus.read(0x06018888, 2).value, 0xABCDu);
}

TEST(bus, open_bus_returns_last_driven_value) {
    auto bus_storage = std::make_unique<Bus>();
    Bus& bus = *bus_storage;
    const BusResult first = bus.read(0x10000000, 4);   // unmapped
    EXPECT_EQ(first.value, 0u);           // reset latch
    EXPECT_EQ(first.cycles, 1u);
    bus.write(0x03000020, 4, 0xCAFED00Du);
    const BusResult driven = bus.read(0x03000020, 4);
    EXPECT_EQ(driven.value, 0xCAFED00Du);
    const BusResult again = bus.read(0x20000000, 4);   // unmapped
    EXPECT_EQ(again.value, 0xCAFED00Du);  // last value on the data bus
}

TEST(hidden, bus_hidden_sram_is_byte_wide) {
    auto bus_storage = std::make_unique<Bus>();
    Bus& bus = *bus_storage;
    bus.write(0x0E000010, 4, 0xAABBCCDDu);
    // Only the low byte lands (8-bit bus).
    EXPECT_EQ(bus.read(0x0E000010, 1).value, 0xDDu);
    EXPECT_EQ(bus.read(0x0E000010, 4).value, 0xDDu);  // byte-wide answer
}
