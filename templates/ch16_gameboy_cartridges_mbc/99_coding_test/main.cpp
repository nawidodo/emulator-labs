// Tests for 99_coding_test: MBC-X spec examples (see CODING_TEST.md).
#define LABSTEST_MAIN
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "labstest.hpp"
#include "unseen_mapper.hpp"

namespace {

// Two-bank image: bank k filled with byte k's marker.
std::vector<uint8_t> makeMbcxRom() {
    std::vector<uint8_t> rom(0x8000, 0x00);
    std::memset(rom.data() + 0x4000, 0x2A, 0x4000);   // bank 1 marker
    rom[0x147] = mbcx::kTypeCode;
    return rom;
}

}  // namespace

TEST(mbcx, write_decode_three_bit_bank_and_open_bus_switch) {
    auto rom = makeMbcxRom();
    mbcx::MbcX m(rom.data(), rom.size());
    EXPECT_EQ(m.reg1(), 1);              // R1 resets to 1
    EXPECT_FALSE(m.reg2());              // R2 resets to 0

    m.writeReg(0x2000, 0x05);
    EXPECT_EQ(m.reg1(), 5);
    m.writeReg(0x2000, 0xFF);            // masked to 3 bits
    EXPECT_EQ(m.reg1(), 7);
    m.writeReg(0x2000, 0x00);            // 0 -> 1 like real banks
    EXPECT_EQ(m.reg1(), 1);

    m.writeReg(0x4000, 0x01);
    EXPECT_TRUE(m.reg2());
    m.writeReg(0x4000, 0x00);
    EXPECT_FALSE(m.reg2());

    // Every other window is ignored.
    m.writeReg(0x0000, 0xFF);
    m.writeReg(0x6000, 0xFF);
    m.writeReg(0xA000, 0xFF);            // routed through writeRam: no-op
    EXPECT_EQ(m.reg1(), 1);
    EXPECT_FALSE(m.reg2());
}

TEST(mbcx, reads_follow_the_spec_examples) {
    auto rom = makeMbcxRom();
    mbcx::MbcX m(rom.data(), rom.size());
    // $0000-$3FFF always bank 0.
    EXPECT_EQ(m.readRom(0x0100), 0x00);
    // Default R1=1: high half shows the bank-1 marker.
    EXPECT_EQ(m.readRom(0x4000), 0x2A);
    EXPECT_EQ(m.readRom(0x7FFF), 0x2A);

    m.writeReg(0x2000, 0x07);
    // Only two physical banks exist; offset 7*0x4000 exceeds the image,
    // so the read falls on open bus ($FF) per the out-of-range rule.
    EXPECT_EQ(m.readRom(0x5000), 0xFF);

    m.writeReg(0x2000, 0x01);
    m.writeReg(0x4000, 0x01);            // soft open bus wins
    EXPECT_EQ(m.readRom(0x4000), 0xFF);
    m.writeReg(0x4000, 0x02);            // only bit 0 matters
    EXPECT_EQ(m.readRom(0x4000), 0x2A);
}

TEST(mbcx, no_ram_and_factory_dispatch) {
    auto rom = makeMbcxRom();
    mbcx::MbcX m(rom.data(), rom.size());
    EXPECT_EQ(m.readRam(0xA000), 0xFF);  // no RAM chip at all

    const auto owned =
        std::unique_ptr<mbcx::Mapper>(mbcx::makeMapper(rom.data(),
                                                       rom.size()));
    EXPECT_TRUE(owned.get() != nullptr);
    rom[0x147] = 0x19;
    EXPECT_TRUE(mbcx::makeMapper(rom.data(), rom.size()) == nullptr);
    EXPECT_TRUE(mbcx::makeMapper(rom.data(), 0x10) == nullptr);  // tiny
}
