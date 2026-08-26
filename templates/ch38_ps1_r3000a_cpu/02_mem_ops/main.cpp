#define LABSTEST_MAIN
#include "labstest.hpp"
#include <memory>
#include "memops.hpp"

using psx::r3000a::Bus;

TEST(words, roundtrip) {
    auto bus_storage = std::make_unique<Bus>();
    Bus& bus = *bus_storage;
    psx::r3000a::do_sw(bus, 0x80000100u, 0x12345678u);
    EXPECT_EQ(psx::r3000a::do_lw(bus, 0x00000100u), 0x12345678u);   // kuseg mirror
    EXPECT_EQ(psx::r3000a::do_lw(bus, 0xA0000100u), 0x12345678u);   // kseg1 mirror
}

TEST(bytes, sign_extension) {
    auto bus_storage = std::make_unique<Bus>();
    Bus& bus = *bus_storage;
    psx::r3000a::do_sb(bus, 0x100u, 0x80u);
    EXPECT_EQ(psx::r3000a::do_load_byte(bus, 0x100u, false), 0x80u);
    EXPECT_EQ(psx::r3000a::do_load_byte(bus, 0x100u, true), 0xFFFFFF80u);
    psx::r3000a::do_sb(bus, 0x101u, 0x7Fu);
    EXPECT_EQ(psx::r3000a::do_load_byte(bus, 0x101u, true), 0x7Fu);
}

TEST(halfwords, sign_extension) {
    auto bus_storage = std::make_unique<Bus>();
    Bus& bus = *bus_storage;
    psx::r3000a::do_sh(bus, 0x100u, 0x8001u);
    EXPECT_EQ(psx::r3000a::do_load_half(bus, 0x100u, false), 0x8001u);
    EXPECT_EQ(psx::r3000a::do_load_half(bus, 0x100u, true), 0xFFFF8001u);
}

namespace {
// Reference: the canonical two-instruction unaligned load of the word at x.
uint32_t ref_unaligned_load(const Bus& bus, uint32_t x) {
    auto b = [&](uint32_t i) { return bus.read8(i); };
    return uint32_t(b(x)) | uint32_t(b(x + 1)) << 8 | uint32_t(b(x + 2)) << 16 |
           uint32_t(b(x + 3)) << 24;
}
}  // namespace

TEST(lwlr, all_alignments) {
    for (uint32_t off = 0; off < 4; ++off) {
        auto bus_storage = std::make_unique<Bus>();
        Bus& bus = *bus_storage;
        // four consecutive marker words
        psx::r3000a::do_sw(bus, 0x200u, 0x11111111u);
        psx::r3000a::do_sw(bus, 0x204u, 0x22222222u);
        psx::r3000a::do_sw(bus, 0x208u, 0x33333333u);
        psx::r3000a::do_sw(bus, 0x20Cu, 0x44444444u);
        const uint32_t x = 0x204u + off;
        const uint32_t expect = ref_unaligned_load(bus, x);
        // lwr $t0, 0(x) ; lwl $t0, 3(x)
        uint32_t t0 = 0xDEADBEEFu;  // garbage must be fully overwritten
        t0 = psx::r3000a::do_lwr(bus, x, t0);
        t0 = psx::r3000a::do_lwl(bus, x + 3u, t0);
        EXPECT_EQ(t0, expect);
    }
}

TEST(lwlr, preserves_untouched_halves) {
    auto bus_storage = std::make_unique<Bus>();
    Bus& bus = *bus_storage;
    psx::r3000a::do_sw(bus, 0x200u, 0xAABBCCDDu);
    // lwr with b=2 loads only the low 16 bits; high half of rt survives.
    const uint32_t t0 = psx::r3000a::do_lwr(bus, 0x202u, 0x11223344u);
    EXPECT_EQ(t0, 0x1122AABBu);
    // lwl with b=1 pulls word bytes 1,0 into the TOP two bytes of rt
    // (little-endian rule); rt's low half survives untouched.
    const uint32_t t1 = psx::r3000a::do_lwl(bus, 0x201u, 0x55667788u);
    EXPECT_EQ(t1, 0xCCDD7788u);
}
