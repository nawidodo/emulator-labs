#define LABSTEST_MAIN
#include <vector>
#include "labstest.hpp"
#include "eeprom.hpp"

using namespace gba;

namespace {

// Build a DMA3-style write stream: start 1, op "00", addr, 64 data bits,
// stop 0. Returns the halfwords to feed.
std::vector<u16> make_write_stream(int addr_width, u32 addr, u64 data) {
    std::vector<int> bits = {1, 0, 0};
    for (int i = addr_width - 1; i >= 0; --i) bits.push_back((addr >> i) & 1);
    for (int i = 63; i >= 0; --i) bits.push_back((int(data >> i) & 1));
    bits.push_back(0);
    // pack bits into u16 stream MSB-first (pad last word with zeros)
    while (bits.size() % 16) bits.push_back(0);
    std::vector<u16> words;
    for (size_t i = 0; i < bits.size(); i += 16) {
        u16 w = 0;
        for (int b = 0; b < 16; ++b) w = u16(w | bits[i + b] << (15 - b));
        words.push_back(w);
    }
    return words;
}

std::vector<u16> make_read_stream(int addr_width, u32 addr) {
    std::vector<int> bits = {1, 1, 0};
    for (int i = addr_width - 1; i >= 0; --i) bits.push_back((addr >> i) & 1);
    bits.push_back(0);  // stop before reading
    while (bits.size() % 16) bits.push_back(0);
    std::vector<u16> words;
    for (size_t i = 0; i < bits.size(); i += 16) {
        u16 w = 0;
        for (int b = 0; b < 16; ++b) w = u16(w | bits[i + b] << (15 - b));
        words.push_back(w);
    }
    return words;
}

}  // namespace

TEST(eeprom, write_then_read_512b) {
    Eeprom e(kEeprom512B);
    auto wr = make_write_stream(6, 7, 0xDEADBEEFCAFEBABEull);
    e.feed_dma_stream(wr.data(), int(wr.size()));
    e.stop();

    auto rd = make_read_stream(6, 7);
    e.feed_dma_stream(rd.data(), int(rd.size()));
    u64 got = 0;
    for (int i = 0; i < 64; ++i) got = (got << 1) | u64(e.read_bit());
    EXPECT_EQ(got, 0xDEADBEEFCAFEBABEull);
}

TEST(eeprom, wide_addressing_8k) {
    Eeprom e(kEeprom8KB);
    const u32 addr = 500;
    auto wr = make_write_stream(14, addr, 0x123456789ABCDEF0ull);
    e.feed_dma_stream(wr.data(), int(wr.size()));
    e.stop();
    auto rd = make_read_stream(14, addr);
    e.feed_dma_stream(rd.data(), int(rd.size()));
    u64 got = 0;
    for (int i = 0; i < 64; ++i) got = (got << 1) | u64(e.read_bit());
    EXPECT_EQ(got, 0x123456789ABCDEF0ull);
}

TEST(eeprom, stray_start_bit_ignored_in_frame) {
    Eeprom e(kEeprom512B);
    // Feeding zeros forever keeps the device idle.
    for (int i = 0; i < 100; ++i) e.feed(0);
    EXPECT_EQ(e.state, Eeprom::State::Idle);
    // A read of untouched (erased) memory returns all one bits.
    auto rd = make_read_stream(6, 3);
    e.feed_dma_stream(rd.data(), int(rd.size()));
    int ones = 0;
    for (int i = 0; i < 64; ++i) ones += e.read_bit();
    EXPECT_EQ(ones, 64);
}
