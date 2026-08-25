// Tests for 01_range_table: routing, boundary exactness, and the
// documented unmapped-gap policy. Hidden grading filters suites by the
// "routing." prefix.
#define LABSTEST_MAIN
#include <cstdint>

#include "labstest.hpp"
#include "bus.hpp"

using gbmap::Bus;
using gbmap::Ram;

namespace {

// Production-shaped subset of the GB map; echo/OAM/IE arrive in later
// exercises, and FEA0-FEFF plus FFFF are deliberately left unattached so
// the open-bus policy has real gaps to guard.
struct Regions {
    Ram cart{0x0000, 0x7FFF};
    Ram vram{0x8000, 0x9FFF};
    Ram eram{0xA000, 0xBFFF};
    Ram wram{0xC000, 0xDFFF};
    Ram io{0xFF00, 0xFF7F};
    Ram hram{0xFF80, 0xFFFE};
};

Bus& wire(Bus& bus, Regions& r) {
    bus.attach(0x0000, 0x7FFF, &r.cart);
    bus.attach(0x8000, 0x9FFF, &r.vram);
    bus.attach(0xA000, 0xBFFF, &r.eram);
    bus.attach(0xC000, 0xDFFF, &r.wram);
    bus.attach(0xFF00, 0xFF7F, &r.io);
    bus.attach(0xFF80, 0xFFFE, &r.hram);
    return bus;
}

}  // namespace

TEST(routing, first_match_wins_per_region) {
    Regions r;
    Bus bus;
    wire(bus, r);
    EXPECT_EQ(bus.findRange(0x0000)->device, static_cast<gbmap::Device*>(&r.cart));
    EXPECT_EQ(bus.findRange(0x150)->device, static_cast<gbmap::Device*>(&r.cart));
    EXPECT_EQ(bus.findRange(0x8000)->device, static_cast<gbmap::Device*>(&r.vram));
    EXPECT_EQ(bus.findRange(0x9FFF)->device, static_cast<gbmap::Device*>(&r.vram));
    EXPECT_EQ(bus.findRange(0xA000)->device, static_cast<gbmap::Device*>(&r.eram));
    EXPECT_EQ(bus.findRange(0xC000)->device, static_cast<gbmap::Device*>(&r.wram));
    EXPECT_EQ(bus.findRange(0xDFFF)->device, static_cast<gbmap::Device*>(&r.wram));
    EXPECT_EQ(bus.findRange(0xFF00)->device, static_cast<gbmap::Device*>(&r.io));
    EXPECT_EQ(bus.findRange(0xFF80)->device, static_cast<gbmap::Device*>(&r.hram));
    EXPECT_EQ(bus.findRange(0xFFFE)->device, static_cast<gbmap::Device*>(&r.hram));
}

TEST(routing, boundaries_are_inclusive_both_ends) {
    Regions r;
    Bus bus;
    wire(bus, r);
    // One below, first, last, one above — for a sample of ranges.
    EXPECT_TRUE(bus.findRange(0x7FFF) != nullptr);   // cart last byte
    EXPECT_TRUE(bus.findRange(0x8000) != nullptr);   // vram first byte
    EXPECT_TRUE(bus.findRange(0xBFFF) != nullptr);   // eram last byte
    EXPECT_TRUE(bus.findRange(0xC000) != nullptr);   // wram first byte
    const auto* gap = bus.findRange(0xFEA0);         // unusable page: no entry
    EXPECT_TRUE(gap == nullptr);
    EXPECT_TRUE(bus.findRange(0xFEFF) == nullptr);
    EXPECT_TRUE(bus.findRange(0xFFFF) == nullptr);   // IE not attached here
}

TEST(routing, table_stays_ordered_after_scrambled_attach) {
    Regions r;
    Bus bus;
    // Attach in reverse; table must still ascend by base address.
    bus.attach(0xFF80, 0xFFFE, &r.hram);
    bus.attach(0xFF00, 0xFF7F, &r.io);
    bus.attach(0xC000, 0xDFFF, &r.wram);
    bus.attach(0x0000, 0x7FFF, &r.cart);
    uint16_t prev = 0;
    for (const auto& e : bus.table()) {
        EXPECT_TRUE(e.lo >= prev);
        prev = e.lo;
        EXPECT_TRUE(e.hi >= e.lo);
    }
}

TEST(routing, read_write_roundtrip_through_every_region) {
    Regions r;
    Bus bus;
    wire(bus, r);
    const std::pair<uint16_t, gbmap::Ram*> probes[] = {
        {0x0100, &r.cart}, {0x8143, &r.vram}, {0xA123, &r.eram},
        {0xC456, &r.wram}, {0xFF10, &r.io},   {0xFFCC, &r.hram},
    };
    for (const auto& [addr, dev] : probes) {
        const uint8_t marker = static_cast<uint8_t>(addr ^ 0x5B);
        bus.write(addr, marker);
        EXPECT_EQ(bus.read(addr), marker);            // via the bus...
        EXPECT_EQ(dev->cells()[addr - dev->lo()], marker);  // ...landed in the right device
    }
}

TEST(routing, devices_stay_isolated) {
    Regions r;
    Bus bus;
    wire(bus, r);
    // Hammer VRAM with a sentinel, then prove nothing else moved.
    for (uint16_t a = 0x8000; a <= 0x9FFF; ++a) bus.write(a, 0xA5);
    const gbmap::Ram* others[] = {&r.cart, &r.eram, &r.wram, &r.io, &r.hram};
    for (const auto* dev : others)
        for (size_t i = 0; i < dev->cells().size(); ++i)
            if (dev->cells()[i] != 0x00) {
                EXPECT_EQ(dev->cells()[i], 0x00);
                return;  // first stray byte fails the test with context
            }
    EXPECT_TRUE(true);
}

TEST(routing, unmapped_gaps_read_documented_open_bus) {
    Regions r;
    Bus bus;
    wire(bus, r);
    // FEA0-FEFF (unusable page) and FFFF (IE lives there in ex02) are
    // unattached: reads must return the documented $00, not float.
    EXPECT_EQ(gbmap::kOpenBusByte, 0x00);
    EXPECT_EQ(bus.read(0xFEA0), 0x00);
    EXPECT_EQ(bus.read(0xFECB), 0x00);
    EXPECT_EQ(bus.read(0xFEFF), 0x00);
    EXPECT_EQ(bus.read(0xFFFF), 0x00);

    Bus empty;  // nothing attached at all: same policy everywhere
    EXPECT_EQ(empty.read(0x0000), 0x00);
    EXPECT_EQ(empty.read(0xFFFF), 0x00);
}

TEST(routing, unmapped_gap_writes_are_dropped) {
    Regions r;
    Bus bus;
    wire(bus, r);
    bus.write(0xFE00 + 0xEE, 0x99);  // 0xFEEE inside the unusable page
    bus.write(0xFEFF, 0x42);
    bus.write(0xFFFF, 0x77);
    // Nothing to corrupt: gap reads stay $00 and neighbours are untouched.
    EXPECT_EQ(bus.read(0xFEAE), 0x00);
    EXPECT_EQ(bus.read(0xFEFF), 0x00);
    EXPECT_EQ(bus.read(0xFFFF), 0x00);
    EXPECT_EQ(r.hram.cells()[0], 0x00);          // FF80 did not absorb it
    EXPECT_EQ(r.io.cells()[0x7F], 0x00);         // FF7F did not absorb it
}

TEST(routing, overlay_priority_is_first_match_not_last) {
    Regions r;
    Bus bus;
    wire(bus, r);
    Ram shadowPage{0x0000, 0x00FF};
    bus.attachFront(0x0000, 0x00FF, &shadowPage);  // overlaid IN FRONT of cart
    EXPECT_EQ(bus.findRange(0x0042)->device, static_cast<gbmap::Device*>(&shadowPage));
    EXPECT_EQ(bus.findRange(0x0142)->device, static_cast<gbmap::Device*>(&r.cart));

    Ram lateOverlay{0x0000, 0x00FF};
    bus.attach(0x0000, 0x00FF, &lateOverlay);  // appended BEHIND: must NOT win
    EXPECT_EQ(bus.findRange(0x0042)->device, static_cast<gbmap::Device*>(&shadowPage));

    EXPECT_EQ(bus.detach(&shadowPage), size_t{1});  // overlay removed: cart shows through
    EXPECT_EQ(bus.findRange(0x0042)->device, static_cast<gbmap::Device*>(&r.cart));
}
