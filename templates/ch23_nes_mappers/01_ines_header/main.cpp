#define LABSTEST_MAIN
#include "labstest.hpp"

#include <string>
#include <vector>

#include "ines.hpp"
#include "mapper.hpp"

using namespace nes23cart;

namespace {

// Synthetic iNES image builders (no commercial ROMs: we assemble our own).
std::vector<uint8_t> make_blob(int mapper, uint8_t prg_banks, uint8_t chr_banks,
                               bool trainer = false, bool four_screen = false,
                               bool vertical = false) {
    std::vector<uint8_t> b = {'N', 'E', 'S', 0x1A, prg_banks, chr_banks, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0};
    if (vertical) b[6] |= 0x01;
    if (trainer) b[6] |= 0x04;
    if (four_screen) b[6] |= 0x08;
    b[6] |= uint8_t((mapper & 0x0F) << 4);
    b[7] |= uint8_t(mapper & 0xF0);
    if (trainer) b.insert(b.end(), 512, 0xEE);
    b.insert(b.end(), size_t(prg_banks) * 16384, 0xA7);
    for (uint8_t bank = 0; bank < chr_banks; ++bank)
        b.insert(b.end(), 8192, uint8_t(0x40 + bank));
    return b;
}

}  // namespace

TEST(nes23cart, header_magic_is_required) {
    Header h;
    std::vector<uint8_t> bad(32, 0);
    bad[0] = 'n';  // lowercase: not the magic
    EXPECT_FALSE(parse_header(bad.data(), bad.size(), h));
    Cart c; std::string err;
    EXPECT_FALSE(load_cart(bad, c, err));
}

TEST(nes23cart, mapper_number_merges_both_nibbles) {
    // Mapper 2 (UxROM): high nibble of flags6.
    auto ux = make_blob(2, 2, 0);
    Cart c; std::string err;
    EXPECT_TRUE(load_cart(ux, c, err));
    EXPECT_EQ(c.mapper, 2);

    // Mapper 19 needs BOTH sources: low nibble of flags6 + nibble of flags7.
    auto both = make_blob(19, 1, 1);
    Cart c19;
    EXPECT_TRUE(load_cart(both, c19, err));
    EXPECT_EQ(c19.mapper, 19);

    // Mapper 18 lives entirely in flags7 (high nibble there).
    auto f7 = make_blob(18, 1, 0);
    Cart c18;
    EXPECT_TRUE(load_cart(f7, c18, err));
    EXPECT_EQ(c18.mapper, 18);
}

TEST(nes23cart, mirroring_bits_and_four_screen_override) {
    Cart c; std::string err;
    EXPECT_TRUE(load_cart(make_blob(0, 1, 1, false, false, true), c, err));
    EXPECT_TRUE(c.mirroring == Mirroring::Vertical);
    EXPECT_TRUE(load_cart(make_blob(0, 1, 1), c, err));
    EXPECT_TRUE(c.mirroring == Mirroring::Horizontal);
    // Four-screen beats whatever bit 0 says.
    EXPECT_TRUE(load_cart(make_blob(0, 1, 1, false, true, true), c, err));
    EXPECT_TRUE(c.mirroring == Mirroring::FourScreen);
}

TEST(nes23cart, trainer_is_skipped_before_prg) {
    auto blob = make_blob(1, 1, 1, /*trainer=*/true);
    Cart c; std::string err;
    EXPECT_TRUE(load_cart(blob, c, err));
    EXPECT_EQ(c.prg.size(), size_t(16384));
    EXPECT_EQ(c.chr.size(), size_t(8192));
    // PRG starts right after trainer; its fill byte proves alignment.
    for (uint8_t v : c.prg) EXPECT_EQ(v, 0xA7);
    for (uint8_t v : c.chr) EXPECT_EQ(v, 0x40);
}

TEST(nes23cart, truncated_image_is_rejected) {
    auto blob = make_blob(3, 2, 1);
    blob.resize(blob.size() - 1);  // one CHR byte short
    Cart c; std::string err;
    EXPECT_FALSE(load_cart(blob, c, err));
}

TEST(nes23cart, chr_ram_boards_have_empty_chr) {
    Cart c; std::string err;
    EXPECT_TRUE(load_cart(make_blob(2, 4, 0), c, err));
    EXPECT_EQ(c.chr.size(), size_t(0));  // UxROM: CHR RAM, 8 KiB on board
}
