#define LABSTEST_MAIN
#include "labstest.hpp"

#include "bus.hpp"

using namespace psx::r3000a;

TEST(memmap, segment_classification) {
    EXPECT_EQ(segment_of(0x00000000u), Segment::Kuseg);
    EXPECT_EQ(segment_of(0x7FFFFFFFu), Segment::Kuseg);
    EXPECT_EQ(segment_of(0x80000000u), Segment::Kseg0);
    EXPECT_EQ(segment_of(0x9FFFFFFFu), Segment::Kseg0);
    EXPECT_EQ(segment_of(0xA0000000u), Segment::Kseg1);
    EXPECT_EQ(segment_of(0xBFC00000u), Segment::Kseg1);
    EXPECT_EQ(segment_of(0xC0000000u), Segment::Kseg2);
}

TEST(memmap, physical_address_strips_segment_bits) {
    EXPECT_EQ(physical_address(0xBFC00000u), 0x1FC00000u);
    EXPECT_EQ(physical_address(0x9F800000u), 0x1F800000u);
    EXPECT_EQ(physical_address(0x80001234u), 0x00001234u);
    EXPECT_EQ(physical_address(0x00001234u), 0x00001234u);
}

TEST(memmap, cacheability_by_segment) {
    // Same physical cell, different attribute depending on the door used.
    EXPECT_EQ(cache_attr(Segment::Kuseg), CacheAttr::Cached);
    EXPECT_EQ(cache_attr(Segment::Kseg0), CacheAttr::Cached);
    EXPECT_EQ(cache_attr(Segment::Kseg1), CacheAttr::Uncached);
}

TEST(bus, ram_visible_through_all_three_mirrors) {
    Bus bus;
    EXPECT_TRUE((bus_write<uint32_t>(&bus, 0x80000100, 0xDEADBEEFu)));
    uint32_t v = 0;
    EXPECT_TRUE(bus_read(&bus, 0x00000100, &v));
    EXPECT_EQ(v, 0xDEADBEEFu);
    v = 0;
    EXPECT_TRUE(bus_read(&bus, 0xA0000100, &v));
    EXPECT_EQ(v, 0xDEADBEEFu);

    CacheAttr a{};
    bus_read(&bus, 0xA0000100, &v, &a);   // uncached alias of the same RAM
    EXPECT_EQ(a, CacheAttr::Uncached);
    bus_read(&bus, 0x80000100, &v, &a);
    EXPECT_EQ(a, CacheAttr::Cached);
}

TEST(bus, ram_wraps_every_2mb_within_8mb_window) {
    Bus bus;
    bus_write(&bus, 0x80000010u, uint32_t{0x12345678});
    uint32_t v = 0;
    // +2MB and +4MB and +6MB all hit the same physical cell.
    EXPECT_TRUE(bus_read(&bus, 0x80200010u, &v));
    EXPECT_EQ(v, 0x12345678u);
    EXPECT_TRUE(bus_read(&bus, 0x80400010u, &v));
    EXPECT_EQ(v, 0x12345678u);
    EXPECT_TRUE(bus_read(&bus, 0x80600010u, &v));
    EXPECT_EQ(v, 0x12345678u);
    // Beyond the 8MB window nothing decodes until the scratchpad.
    EXPECT_FALSE(bus_read(&bus, 0x80800010u, &v));
}

TEST(bus, scratchpad_1kb_no_kseg1_mirror) {
    Bus bus;
    bus_write(&bus, 0x1F800000u, uint16_t{0xCAFE});
    uint16_t h = 0;
    EXPECT_TRUE(bus_read(&bus, 0x9F800000u, &h));  // kseg0 works
    EXPECT_EQ(h, 0xCAFEu);
    // PSX-SPX memory map: scratchpad has NO KSEG1 column -> A/B segment
    // aliases fall through to nothing.
    EXPECT_FALSE(bus_read(&bus, 0xBF800000u, &h));
}

TEST(bus, bios_rom_uncached_readonly) {
    const uint8_t image[] = {0x40, 0x08, 0x00, 0x3C};  // lui $t0,... LE
    Bus bus;
    bus.load_bios(image);
    uint32_t w = 0;
    EXPECT_TRUE(bus_read(&bus, 0xBFC00000u, &w));
    EXPECT_EQ(w, 0x3C000840u);
    EXPECT_FALSE(bus_write(&bus, 0xBFC00000u, uint32_t{0}));  // ROM ignores
    EXPECT_TRUE(bus_read(&bus, 0xBFC00000u, &w));             // unchanged
    EXPECT_EQ(w, 0x3C000840u);
}

TEST(bus, memory_control_defaults_and_rw) {
    Bus bus;
    uint32_t v = 0;
    EXPECT_TRUE(bus_read(&bus, 0x1F801000u, &v));   // EXP1 base
    EXPECT_EQ(v, 0x1F000000u);
    EXPECT_TRUE(bus_read(&bus, 0x1F801008u, &v));   // EXP1 delay/size
    EXPECT_EQ(v, 0x0013243Fu);
    EXPECT_TRUE(bus_read(&bus, 0x1F801060u, &v));   // RAM_SIZE reset value
    EXPECT_EQ(v, 0x00000B88u);

    // The BIOS rewrites RAM_SIZE to 888h during init; model must remember.
    bus_write(&bus, 0x1F801060u, uint32_t{0x00000888});
    bus_read(&bus, 0x1F801060u, &v);
    EXPECT_EQ(v, 0x00000888u);

    EXPECT_TRUE(bus_read(&bus, 0x1F80103Cu, &v));   // undocumented port
    EXPECT_EQ(v, 0u);
}
