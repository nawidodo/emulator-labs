// Tests for 04_boot_rom: overlay at reset, unmap on ANY FF50 write,
// and the rest of the map keeps working underneath. Hidden grading
// filters suites by the "boot." prefix (these suites also cover echo
// aliasing inside the boot machine).
#define LABSTEST_MAIN
#include <cstdint>
#include <vector>

#include "labstest.hpp"
#include "boot_bus.hpp"

using gbboot::BootMachine;
using gbboot::BootImage;

namespace {

std::vector<uint8_t> makeCart() {
    std::vector<uint8_t> img(0x8000);
    for (size_t i = 0; i < img.size(); ++i)
        img[i] = static_cast<uint8_t>(i * 7 + 0x11);
    return img;
}

}  // namespace

TEST(boot, reset_maps_overlay_over_cartridge) {
    const BootImage bootImg = gbboot::makeSyntheticBoot();
    BootMachine m(makeCart());
    // First page: boot image wins.
    EXPECT_EQ(m.bus.read(0x0000), bootImg[0x000]);
    EXPECT_EQ(m.bus.read(0x0042), bootImg[0x042]);
    EXPECT_EQ(m.bus.read(0x00FF), bootImg[0x0FF]);
    // One byte past the overlay: cartridge shows through already.
    EXPECT_EQ(m.bus.read(0x0100), static_cast<uint8_t>(0x0100 * 7 + 0x11));
    EXPECT_TRUE(m.unmapper.bootMapped());
}

TEST(boot, any_ff50_write_unmaps) {
    const uint8_t kAnyValues[] = {0x00, 0x01, 0x50, 0xA5, 0xFF};
    for (uint8_t v : kAnyValues) {
        BootMachine m(makeCart());
        m.bus.write(gbboot::kBootDisable, v);
        EXPECT_FALSE(m.unmapper.bootMapped());       // value is irrelevant
        auto cart = makeCart();
        EXPECT_EQ(m.bus.read(0x0000), cart[0x0000]);
        EXPECT_EQ(m.bus.read(0x00FF), cart[0x00FF]);
    }
}

TEST(boot, double_unmap_is_harmless) {
    BootMachine m(makeCart());
    m.bus.write(gbboot::kBootDisable, 0x01);
    EXPECT_FALSE(m.unmapper.bootMapped());
    m.bus.write(gbboot::kBootDisable, 0x02);  // second handshake: nothing left to remove
    EXPECT_FALSE(m.unmapper.bootMapped());
    auto cart = makeCart();
    EXPECT_EQ(m.bus.read(0x007F), cart[0x007F]);  // still the cartridge
}

TEST(boot, non_ff50_writes_keep_the_overlay) {
    BootMachine m(makeCart());
    const BootImage bootImg = gbboot::makeSyntheticBoot();
    const uint16_t decoys[] = {0xFF00, 0xFF4F, 0xFF51, 0xFF7F, 0xFF80,
                               0xC000, 0xE000, 0xFFFF};
    for (uint16_t addr : decoys) m.bus.write(addr, 0xFF);
    EXPECT_TRUE(m.unmapper.bootMapped());
    EXPECT_EQ(m.bus.read(0x0042), bootImg[0x042]);  // boot still shadows the cart
    // The decoy writes landed where they belonged:
    EXPECT_EQ(m.bus.read(0xFF4F), 0xFF);  // I/O cell
    EXPECT_EQ(m.bus.read(0xFF80), 0xFF);  // HRAM cell
    EXPECT_EQ(m.bus.read(0xC000), 0xFF);  // WRAM cell
}

TEST(boot, echo_still_aliases_after_unmap) {
    BootMachine m(makeCart());
    m.bus.write(gbboot::kBootDisable, 0x01);
    // Echo window routes into WRAM regardless of the boot state:
    m.bus.write(0xE123, 0x5E);
    EXPECT_EQ(m.bus.read(0xC123), 0x5E);
    m.bus.write(0xC456, 0x6C);
    EXPECT_EQ(m.bus.read(0xE456), 0x6C);
    EXPECT_EQ(m.wram.cells()[0x0123], 0x5E);
}

TEST(boot, unmap_does_not_disturb_the_rest_of_the_map) {
    BootMachine m(makeCart());
    m.bus.write(0xC000 + 9, 0x77);
    m.bus.write(0xFF90, 0x33);
    m.bus.write(gbboot::kBootDisable, 0x01);
    EXPECT_EQ(m.bus.read(0xC000 + 9), 0x77);   // WRAM survives
    EXPECT_EQ(m.bus.read(0xFF90), 0x33);       // HRAM survives
    EXPECT_EQ(m.bus.read(0xE000 + 9), 0x77);   // echo still consistent
}
