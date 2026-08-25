#define LABSTEST_MAIN
#include "labstest.hpp"
#include "golden.hpp"
#include "../01_digital_pad/pad.hpp"
#include "../02_card_protocol/memcard.hpp"
#include "../03_card_image/card_image.hpp"
#include "../03_card_image/sio_bus.hpp"
#include "../shared/fnv.hpp"

#include <array>
#include <vector>

using namespace sio;

// Challenge: a full virtual-memory-card round trip.
//
//   1. build a card image with one directory entry (block 1 in use) and a
//      second block flagged BAD;
//   2. mount it into slot 1 of the dual-slot bus;
//   3. WRITE two pattern sectors through the raw protocol on slot 1's card,
//      and verify a write into the bad block is refused;
//   4. export the card bytes, remount them into a FRESH MemCard;
//   5. READ both sectors back through the protocol and pin every returned
//      byte with FNV-1a 64.
//
// Read-back stream per sector: pre + 3 mid + payload(128) + checksum +
// end flag = 134 bytes after the select byte; two sectors -> 268 bytes.

static constexpr unsigned kSectorA = 0x040;  // inside good block 1
static constexpr unsigned kSectorB = 0x041;

static void fill_pattern(std::array<uint8_t, kSectorSize>& buf, uint8_t seed) {
    for (unsigned i = 0; i < kSectorSize; ++i) {
        buf[i] = static_cast<uint8_t>(seed + i * 3);
    }
}

// Drive a whole card transaction over the bus. `body` excludes the device
// select byte, which this helper transmits itself; returns ALL RX bytes.
static std::vector<uint8_t> card_xfer(SioBus& bus,
                                      const std::vector<uint8_t>& body) {
    std::vector<uint8_t> rx;
    rx.push_back(bus.xfer(kSelectCard));
    for (uint8_t b : body) rx.push_back(bus.xfer(b));
    bus.deselect();
    return rx;
}

static std::vector<uint8_t> write_frame(unsigned sector,
                                        const std::array<uint8_t, kSectorSize>& data) {
    std::vector<uint8_t> w{CMD_WRITE, static_cast<uint8_t>(sector >> 8),
                           static_cast<uint8_t>(sector & 0xFF), 0x00};
    w.insert(w.end(), data.begin(), data.end());
    w.push_back(xor_checksum(data.data(), kSectorSize));
    w.push_back(0x00);  // final clock: end flag
    return w;
}

TEST(challenge_roundtrip, writes_commit_and_remount_preserves_bytes) {
    CardImage img;
    img.reset();
    img.set_entry(1, CardImage::kStateInUse, 2, "CHALLNGE.DAT", false);

    SioBus bus;
    bus.write_ctrl(CTRL_SLOT1);
    img.mount_into(bus.cards[0]);

    std::array<uint8_t, kSectorSize> pa{}, pb{};
    fill_pattern(pa, 0x11);
    fill_pattern(pb, 0x77);

    auto rx = card_xfer(bus, write_frame(kSectorA, pa));
    EXPECT_EQ(rx.size(), size_t(135));  // select FF + 134 frame bytes
    EXPECT_EQ(rx.back(), kFlagGood);

    rx = card_xfer(bus, write_frame(kSectorB, pb));
    EXPECT_EQ(rx.back(), kFlagGood);

    // Export -> remount into a fresh card object.
    std::array<uint8_t, kImageBytes> dump{};
    bus.cards[0].export_image(dump.data());
    MemCard fresh;
    fresh.reset();
    fresh.load_image(dump.data());

    // READ both sectors back through the protocol; concatenate everything
    // after each select byte's FF response.
    std::vector<uint8_t> readback;
    for (unsigned sector : {kSectorA, kSectorB}) {
        std::vector<uint8_t> r{CMD_READ, static_cast<uint8_t>(sector >> 8),
                               static_cast<uint8_t>(sector & 0xFF), 0x00};
        r.insert(r.end(), 130, 0x00);
        auto rrx = card_xfer(bus, r);
        readback.insert(readback.end(), rrx.begin() + 1, rrx.end());
        EXPECT_EQ(rrx.back(), kFlagGood);
    }
    EXPECT_EQ(fresh.sector_data(kSectorB)[0], pb[0]);

    EXPECT_EQ(readback.size(), size_t(268));
    const uint64_t h = fnv64({readback.data(), readback.size()});
    EXPECT_EQ(h, kGoldenRoundtripFnv64);
}

TEST(challenge_roundtrip, bad_block_flag_survives_the_remount) {
    CardImage img;
    img.reset();
    img.set_entry(2, CardImage::kStateInUse, 1, "FLAT.BIN", true);
    SioBus bus;
    bus.write_ctrl(CTRL_SLOT1);
    img.mount_into(bus.cards[0]);
    EXPECT_TRUE(bus.cards[0].is_bad(128));
    EXPECT_TRUE(bus.cards[0].is_bad(191));

    // A write into the bad block (sector 256 = block 4? no: block 2 starts
    // at sector 128; use sector 0x100 inside block 2? block 2 = sectors
    // 128..191 = 0x80..0xBF) must be refused and change nothing.
    std::array<uint8_t, kSectorSize> data{};
    data.fill(0xE3);
    auto rx = card_xfer(bus, write_frame(0x00A0, data));  // block 2 interior
    EXPECT_EQ(rx.back(), kFlagBad);

    // And a read of it answers all-FF / chk 00 / bad flag.
    std::vector<uint8_t> r{CMD_READ, 0x00, 0xA0, 0x00};
    r.insert(r.end(), 130, 0x00);
    rx = card_xfer(bus, r);
    EXPECT_EQ(rx.size(), size_t(135));
    for (unsigned i = 0; i < kSectorSize; ++i) EXPECT_EQ(rx[5 + i], 0xFF);
    EXPECT_EQ(rx[133], 0x00);
    EXPECT_EQ(rx[134], kFlagBad);
}
