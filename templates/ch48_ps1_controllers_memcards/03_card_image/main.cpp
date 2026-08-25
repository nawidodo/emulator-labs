#define LABSTEST_MAIN
#include "labstest.hpp"
#include "card_image.hpp"
#include "sio_bus.hpp"
#include "../shared/fnv.hpp"

#include <algorithm>
#include <array>
#include <vector>

using namespace sio;

static CardImage image_with_bad_block() {
    CardImage img;
    img.reset();
    img.set_entry(1, CardImage::kStateInUse, 2, "GOOD.DAT", false);
    img.set_entry(2, CardImage::kStateInUse, 1, "BADONE.DAT", true);
    return img;
}

TEST(card_image, geometry_constants) {
    EXPECT_EQ(kSectorSize, 128u);
    EXPECT_EQ(kSectorCount, 1024u);
    EXPECT_EQ(kImageBytes, 131072u);   // standard .mcr file size
    EXPECT_EQ(kBlockSectors, 64u);     // one curriculum "block" = 8 KiB
    EXPECT_EQ(kBlockCount, 16u);
    EXPECT_EQ(CardImage::kSize, kImageBytes);
}

TEST(card_image, reset_writes_mc_magic) {
    CardImage img;
    img.reset();
    EXPECT_EQ(img.bytes()[0], 'M');
    EXPECT_EQ(img.bytes()[1], 'C');
    EXPECT_EQ(img.bytes()[2], 0xFF);
    EXPECT_EQ(img.bytes().size(), size_t(131072));
}

TEST(card_image, entry_fields_landed_where_documented) {
    CardImage img;
    img.reset();
    img.set_entry(3, CardImage::kStateInUse, 5, "SAVE.BIN", false);
    const uint8_t* e = img.entry(3);
    EXPECT_EQ(e[0], 0xA0);
    EXPECT_EQ(e[2], 5);
    EXPECT_EQ(e[3], 0);
    EXPECT_EQ(e[4], 'S');
    EXPECT_EQ(e[11], 'N');
    EXPECT_EQ(img.entry(3)[21] & CardImage::kAttrBadBlock, 0);
}

TEST(card_image, directory_scan_marks_whole_block_bad) {
    MemCard card;
    card.reset();
    image_with_bad_block().mount_into(card);
    // Block 2 covers sectors 128..191 — every one flagged bad.
    for (unsigned s = 128; s < 192; ++s) EXPECT_TRUE(card.is_bad(s));
    // Block 1 stays good.
    EXPECT_FALSE(card.is_bad(64));
    EXPECT_FALSE(card.is_bad(127));
    EXPECT_FALSE(card.is_bad(192));
}

// ---- dual-slot bus wiring -------------------------------------------------

TEST(sio_bus, no_slot_enabled_reads_ff) {
    SioBus bus;
    bus.write_ctrl(0);
    EXPECT_EQ(bus.xfer(kSelectPad), 0xFF);
    EXPECT_EQ(bus.xfer(kPadCmdRead), 0xFF);
    EXPECT_EQ(bus.active_slot(), -1);
}

TEST(sio_bus, slot_bits_route_to_independent_pads) {
    SioBus bus;
    Buttons b;
    b.cross = true;
    bus.pads[1].set_buttons(b);

    bus.write_ctrl(CTRL_SLOT2);  // slot 2
    bus.xfer(kSelectPad);
    bus.xfer(kPadCmdRead);
    EXPECT_EQ(bus.xfer(0x00), kPadIdLo);
    EXPECT_EQ(bus.xfer(0x00), kPadIdHi);
    EXPECT_EQ(bus.xfer(0x00),
              static_cast<uint8_t>(report_word(b) & 0xFF));  // cross -> lo FF

    bus.deselect();
    bus.write_ctrl(CTRL_SLOT1);  // slot 1: its own pad, nothing pressed
    bus.xfer(kSelectPad);
    bus.xfer(kPadCmdRead);
    EXPECT_EQ(bus.xfer(0x00), kPadIdLo);
    EXPECT_EQ(bus.xfer(0x00), kPadIdHi);
    EXPECT_EQ(bus.xfer(0x00), 0xFF);   // nothing pressed on pads[0]
}

TEST(sio_bus, both_slot_bits_slot2_wins) {
    SioBus bus;
    bus.write_ctrl(CTRL_SLOT1 | CTRL_SLOT2);
    EXPECT_EQ(bus.active_slot(), 1);
}

TEST(sio_bus, select_byte_pads_vs_card) {
    SioBus bus;
    bus.write_ctrl(CTRL_SLOT1);
    bus.cards[0].reset();

    // Pad session: 01 arms the pad.
    EXPECT_EQ(bus.xfer(kSelectPad), 0xFF);
    EXPECT_EQ(bus.xfer(kPadCmdRead), 0xFF);
    EXPECT_EQ(bus.xfer(0x00), kPadIdLo);
    bus.deselect();

    // Card session: 81 arms the card, which answers GETID.
    uint8_t tx[] = {kSelectCard, CMD_GETID, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t want[] = {0xFF, kFlagPre, kFlagMid, kFlagMid, kFlagMid,
                      0x04, 0x00,    0x00,     0x80,     0x84, kFlagGood};
    for (int i = 0; i < 11; ++i) EXPECT_EQ(bus.xfer(tx[i]), want[i]);
}

TEST(sio_bus, stat_reflects_ack_level_noninverted) {
    SioBus bus;
    bus.write_ctrl(CTRL_SLOT1);
    EXPECT_EQ(bus.read_stat() & STAT_TXRDY, STAT_TXRDY);
    EXPECT_EQ(bus.read_stat() & STAT_ACK_LEVEL, 0);
    bus.xfer(kSelectCard);
    bus.xfer(CMD_READ);
    bus.xfer(0x00);  // addr msb -> ACK now asserted
    EXPECT_NE(bus.read_stat() & STAT_ACK_LEVEL, 0);
}

TEST(sio_bus, ctrl_reset_bit_resets_devices) {
    SioBus bus;
    bus.write_ctrl(CTRL_SLOT1);
    bus.xfer(kSelectPad);
    bus.xfer(kPadCmdRead);
    bus.xfer(0x00);  // mid-transaction
    bus.write_ctrl(CTRL_SLOT1 | CTRL_RESET);
    EXPECT_EQ(bus.pads[0].selected(), false);
    bus.deselect();
    bus.xfer(kSelectPad);
    EXPECT_EQ(bus.xfer(kPadCmdRead), 0xFF);
    EXPECT_EQ(bus.xfer(0x00), kPadIdLo);  // fresh session after reset
}

TEST(sio_bus, serial_bit_counter_counts_bytes) {
    SioBus bus;
    bus.write_ctrl(CTRL_SLOT1);
    EXPECT_EQ(bus.serial_bits(), 0u);
    bus.xfer(kSelectPad);
    bus.xfer(kPadCmdRead);
    EXPECT_EQ(bus.serial_bits(), 16u);
}

TEST(challenge_seed, protocol_roundtrip_through_the_bus) {
    // Write a sector through the real protocol, export image bytes, remount,
    // read back through the protocol.
    CardImage img;
    img.reset();
    img.set_entry(1, CardImage::kStateInUse, 1, "RT.DAT", false);
    SioBus bus;
    bus.write_ctrl(CTRL_SLOT1);        // arm slot 1 before any transfer
    img.mount_into(bus.cards[0]);

    std::array<uint8_t, kSectorSize> data{};
    for (unsigned i = 0; i < kSectorSize; ++i)
        data[i] = static_cast<uint8_t>(i * 3 + 1);

    std::vector<uint8_t> tx{kSelectCard, CMD_WRITE, 0x00, 0x40, 0x00};
    tx.insert(tx.end(), data.begin(), data.end());
    tx.push_back(xor_checksum(data.data(), kSectorSize));
    tx.push_back(0x00);
    for (uint8_t b : tx) bus.xfer(b);
    bus.deselect();

    std::array<uint8_t, kImageBytes> dump{};
    bus.cards[0].export_image(dump.data());
    MemCard fresh;
    fresh.reset();
    fresh.load_image(dump.data());

    std::vector<uint8_t> rtx{kSelectCard, CMD_READ, 0x00, 0x40, 0x00};
    rtx.insert(rtx.end(), 130, 0x00);
    std::vector<uint8_t> rx;
    fresh.select(true);
    for (uint8_t b : rtx) rx.push_back(fresh.handle(b));
    fresh.select(false);
    EXPECT_EQ(rx.back(), kFlagGood);
    for (unsigned i = 0; i < kSectorSize; ++i) {
        EXPECT_EQ(rx[5 + i], data[i]);
    }
}
