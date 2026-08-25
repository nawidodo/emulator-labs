#define LABSTEST_MAIN
#include "labstest.hpp"
#include "debug_memcard.hpp"
#include "debug_pad.hpp"

#include <array>
#include <vector>

using namespace siodbg;

// ---- seeded bug 1: memcard XOR range --------------------------------------

static MemCard pattern_card() {
    MemCard card;
    card.reset();
    for (unsigned s = 0; s < kSectorCount; ++s) {
        for (unsigned i = 0; i < kSectorSize; ++i) {
            card.sector_data(s)[i] = static_cast<uint8_t>((s * 7 + i) & 0xFF);
        }
    }
    return card;
}

TEST(dbgcard_xor, read_checksum_covers_all_128_bytes) {
    MemCard card = pattern_card();
    std::vector<uint8_t> tx{sio::kSelectCard, CMD_READ, 0x00, 0x28, 0x00};
    tx.insert(tx.end(), 130, 0x00);
    card.select(true);
    std::vector<uint8_t> rx;
    for (uint8_t b : tx) rx.push_back(card.handle(b));
    card.select(false);

    uint8_t x = 0;
    for (unsigned i = 0; i < kSectorSize; ++i)
        x = static_cast<uint8_t>(x ^ rx[5 + i]);
    EXPECT_EQ(rx.size(), size_t(135));
    EXPECT_EQ(rx[133], x);   // buggy code answers XOR of bytes 0..126 here
}

TEST(dbgcard_xor, getid_checksum_is_84) {
    MemCard card;
    card.reset();
    uint8_t tx[] = {sio::kSelectCard, CMD_GETID, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    card.select(true);
    uint8_t rx[11];
    for (int i = 0; i < 11; ++i) rx[i] = card.handle(tx[i]);
    card.select(false);
    EXPECT_EQ(rx[9], 0x84);  // 04 ^ 00 ^ 00 ^ 80; buggy code answers 0x04
}

TEST(dbgcard_xor, write_rejects_corruption_of_the_last_byte) {
    // The last payload byte participates in the checksum: flipping ONLY it
    // must make the write fail. The short-range bug never notices.
    MemCard card = pattern_card();
    std::array<uint8_t, kSectorSize> data{};
    for (unsigned i = 0; i < kSectorSize; ++i)
        data[i] = static_cast<uint8_t>(i * 5 + 3);  // last byte != 0
    std::vector<uint8_t> tx{sio::kSelectCard, CMD_WRITE, 0x00, 0x30, 0x00};
    tx.insert(tx.end(), data.begin(), data.end());
    // Host sends the XOR of ONLY bytes 0..126 — valid under the short-range
    // bug, invalid under the correct 128-byte sum because data[127] != 0.
    uint8_t short_chk = 0;
    for (unsigned i = 0; i + 1 < kSectorSize; ++i)
        short_chk = static_cast<uint8_t>(short_chk ^ data[i]);
    tx.push_back(short_chk);
    tx.push_back(0x00);
    card.select(true);
    std::vector<uint8_t> rx;
    for (uint8_t b : tx) rx.push_back(card.handle(b));
    card.select(false);
    EXPECT_EQ(rx.back(), sio::kFlagBad);
}

// ---- seeded bug 2: pad ACK polarity ---------------------------------------

TEST(dbgpad_ack, asserted_while_data_remains) {
    DigitalPad pad;
    pad.set_buttons({});
    pad.select(true);
    pad.handle(kSelectPad);
    pad.handle(kPadCmdRead);
    pad.handle(0x00);          // rx 41
    EXPECT_TRUE(pad.ack());    // inverted bug reports false here
    pad.handle(0x00);          // rx 5A
    EXPECT_TRUE(pad.ack());
}

TEST(dbgpad_ack, low_after_final_byte_and_outside_session) {
    DigitalPad pad;
    pad.set_buttons({});
    pad.select(true);
    EXPECT_FALSE(pad.ack());   // inverted bug reports true here
    pad.handle(kSelectPad);
    EXPECT_FALSE(pad.ack());
    pad.handle(kPadCmdRead);
    pad.handle(0x00);
    pad.handle(0x00);
    pad.handle(0x00);
    pad.handle(0x00);          // final buttons-hi byte
    EXPECT_FALSE(pad.ack());   // inverted bug reports true here
}
