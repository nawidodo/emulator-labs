// Tests for 90_debug: each suite targets exactly one seeded bus
// defect. All three must be fixed before the drill goes green.
#define LABSTEST_MAIN
#include <cstdint>
#include <vector>

#include "labstest.hpp"
#include "bus_debug.hpp"

TEST(debug_echo, writes_land_in_live_wram) {
    std::vector<uint8_t> wram(0x2000, 0x00);  // C000-DFFF
    busdbg::DebugEcho echo(wram);
    // Write through the window, verify the REAL cells moved...
    echo.read(0xE000);  // read path is known-good; touch it so both sides run
    const uint16_t probes[] = {0xE000, 0xE123, 0xEEEE, 0xFDFF};
    const uint8_t marks[] = {0xA1, 0xB2, 0xC3, 0xD4};
    for (size_t i = 0; i < 4; ++i) echo.write(probes[i], marks[i]);
    EXPECT_EQ(wram[0x0000], 0xA1);  // E000 -> C000
    EXPECT_EQ(wram[0x0123], 0xB2);
    EXPECT_EQ(wram[0x0EEE], 0xC3);
    EXPECT_EQ(wram[0x1DFF], 0xD4);  // FDFF -> DDFF, not DFFF
    // ...and reads see exactly what writes put there.
    for (size_t i = 0; i < 4; ++i) EXPECT_EQ(echo.read(probes[i]), marks[i]);
}

TEST(debug_boot, any_ff50_write_unmaps_exactly_once) {
    busdbg::DebugBootMapper m;
    // Any value at FF50 must unmap:
    for (uint8_t v : {0x00u, 0x01u, 0x50u, 0xFFu}) {
        m.bootMapped = true;
        m.onBusWrite(0xFF50, v);
        EXPECT_FALSE(m.bootMapped);
    }
    // Unrelated writes must leave the state alone entirely:
    const uint16_t decoys[] = {0xFF00, 0xFF46, 0xFF4F, 0xFF51, 0xC000};
    for (uint16_t a : decoys) {
        m.bootMapped = true;
        m.onBusWrite(a, 0x99);
        EXPECT_TRUE(m.bootMapped);
        m.bootMapped = false;
        m.onBusWrite(a, 0x99);
        EXPECT_FALSE(m.bootMapped);
    }
}

TEST(debug_gap, unusable_page_reads_stick_to_documented_zero) {
    busdbg::DebugGapBus bus;
    // Hammer the unusable page, then demand $00 back everywhere in it.
    const uint16_t page[] = {0xFEA0, 0xFECB, 0xFEFF, 0xFEF0};
    const uint8_t ink[] = {0xAA, 0xBB, 0xCC, 0xDD};
    for (size_t i = 0; i < 4; ++i) bus.write(page[i], ink[i]);
    for (const uint16_t a : {0xFEA0u, 0xFEA1u, 0xFECBu, 0xFEFFu})
        EXPECT_EQ(bus.read(a), 0x00);
    // The boundary stays intact: FF80 is real HRAM and round-trips.
    bus.write(0xFF80, 0x77);
    bus.write(0xFFFE, 0x88);
    EXPECT_EQ(bus.read(0xFF80), 0x77);
    EXPECT_EQ(bus.read(0xFFFE), 0x88);
}
