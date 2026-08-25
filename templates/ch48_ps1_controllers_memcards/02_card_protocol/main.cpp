#define LABSTEST_MAIN
#include <algorithm>
#include <array>
#include <vector>
#include "labstest.hpp"
#include "memcard.hpp"

using namespace sio;

// Build a card whose sector `s` holds the byte pattern i -> (s*7+i)&0xFF.
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

// Drive a full transaction; returns the RX stream (select byte included).
static std::vector<uint8_t> transact(MemCard& card,
                                     const std::vector<uint8_t>& tx) {
    std::vector<uint8_t> rx;
    card.select(true);
    for (uint8_t b : tx) rx.push_back(card.handle(b));
    card.select(false);
    return rx;
}

TEST(card_xor, known_vectors) {
    uint8_t a = 0x00;
    EXPECT_EQ(xor_checksum(&a, 1), 0x00);
    a = 0xFF;
    EXPECT_EQ(xor_checksum(&a, 1), 0xFF);
    uint8_t two[2] = {0xF0, 0x0F};
    EXPECT_EQ(xor_checksum(two, 2), 0xFF);
    uint8_t seq[4] = {0x01, 0x02, 0x04, 0x08};
    EXPECT_EQ(xor_checksum(seq, 4), 0x0F);
}

TEST(card_xor, sector_pattern_vector) {
    // Pins the checksum of a known 128-byte payload: bytes 0..127.
    uint8_t buf[kSectorSize];
    for (unsigned i = 0; i < kSectorSize; ++i) buf[i] = static_cast<uint8_t>(i);
    uint8_t x = 0;
    for (unsigned i = 0; i < kSectorSize; ++i)
        x = static_cast<uint8_t>(x ^ buf[i]);
    EXPECT_EQ(xor_checksum(buf, kSectorSize), x);
}

TEST(card_read, full_frame_flags_and_payload) {
    MemCard card = pattern_card();
    std::vector<uint8_t> tx{kSelectCard, CMD_READ, 0x00, 0x28, 0x00};
    tx.insert(tx.end(), 130, 0x00);  // 128 payload + chk + end-flag clocks
    const auto rx = transact(card, tx);

    EXPECT_EQ(rx.size(), size_t(135));
    EXPECT_EQ(rx[0], 0xFF);        // select byte
    EXPECT_EQ(rx[1], kFlagPre);    // after 52
    EXPECT_EQ(rx[2], kFlagMid);    // addr msb
    EXPECT_EQ(rx[3], kFlagMid);    // addr mid
    EXPECT_EQ(rx[4], kFlagMid);    // addr lsb
    for (unsigned i = 0; i < kSectorSize; ++i) {
        EXPECT_EQ(rx[5 + i], static_cast<uint8_t>((0x28 * 7 + i) & 0xFF));
    }
    uint8_t x = 0;
    for (unsigned i = 0; i < kSectorSize; ++i)
        x = static_cast<uint8_t>(x ^ rx[5 + i]);
    EXPECT_EQ(rx[133], x);
    EXPECT_EQ(rx[134], kFlagGood);
}

TEST(card_read, address_wraps_at_1024_sectors) {
    MemCard card = pattern_card();
    // a0=0x04 would be sector 0x400: masked to 0x000 by the 10-bit model.
    std::vector<uint8_t> tx{kSelectCard, CMD_READ, 0x04, 0x00, 0x00};
    tx.insert(tx.end(), 130, 0x00);
    const auto rx = transact(card, tx);
    EXPECT_EQ(rx[5], 0x00);        // pattern(0,0)
    EXPECT_EQ(rx[6], 0x01);        // pattern(0,1)
    EXPECT_EQ(rx[134], kFlagGood);
}

TEST(card_read, bad_sector_streams_ff_and_ends_bad) {
    MemCard card = pattern_card();
    card.set_bad(0x123, true);
    std::vector<uint8_t> tx{kSelectCard, CMD_READ, 0x01, 0x23, 0x00};
    tx.insert(tx.end(), 130, 0x00);
    const auto rx = transact(card, tx);
    for (unsigned i = 0; i < kSectorSize; ++i) EXPECT_EQ(rx[5 + i], 0xFF);
    EXPECT_EQ(rx[133], 0x00);      // XOR of 128 x FF is exactly 0x00
    EXPECT_EQ(rx[134], kFlagBad);
}

TEST(card_write, commits_matching_checksum) {
    MemCard card;
    card.reset();
    std::array<uint8_t, kSectorSize> data{};
    for (unsigned i = 0; i < kSectorSize; ++i) data[i] = static_cast<uint8_t>(i ^ 0xA5);
    std::vector<uint8_t> tx{kSelectCard, CMD_WRITE, 0x00, 0x10, 0x00};
    tx.insert(tx.end(), data.begin(), data.end());
    tx.push_back(xor_checksum(data.data(), kSectorSize));
    tx.push_back(0x00);            // final clock for the end flag
    const auto rx = transact(card, tx);

    EXPECT_EQ(rx.size(), size_t(135));
    EXPECT_EQ(rx[1], kFlagPre);
    for (int i = 2; i <= 4; ++i) EXPECT_EQ(rx[i], kFlagMid);
    for (int i = 5; i <= 133; ++i) EXPECT_EQ(rx[i], kFlagMid);  // data + chk
    EXPECT_EQ(rx[134], kFlagGood);
    EXPECT_TRUE(std::equal(data.begin(), data.end(), card.sector_data(0x10)));
}

TEST(card_write, rejects_corrupted_checksum_byte_for_byte) {
    MemCard card = pattern_card();
    const uint8_t before = card.sector_data(3)[42];
    std::array<uint8_t, kSectorSize> data{};
    data.fill(0x5A);
    std::vector<uint8_t> tx{kSelectCard, CMD_WRITE, 0x00, 0x03, 0x00};
    tx.insert(tx.end(), data.begin(), data.end());
    tx.push_back(static_cast<uint8_t>(xor_checksum(data.data(), kSectorSize) ^ 0x40));
    tx.push_back(0x00);
    const auto rx = transact(card, tx);
    EXPECT_EQ(rx[134], kFlagBad);
    // untouched: not one byte may change on a rejected write
    EXPECT_EQ(card.sector_data(3)[42], before);
}

TEST(card_write, bad_sector_discards_data) {
    MemCard card = pattern_card();
    card.set_bad(0x200, true);
    std::array<uint8_t, kSectorSize> data{};
    data.fill(0xC7);
    std::vector<uint8_t> tx{kSelectCard, CMD_WRITE, 0x02, 0x00, 0x00};
    tx.insert(tx.end(), data.begin(), data.end());
    tx.push_back(xor_checksum(data.data(), kSectorSize));
    tx.push_back(0x00);
    const auto rx = transact(card, tx);
    EXPECT_EQ(rx[134], kFlagBad);
    EXPECT_NE(card.sector_data(0x200)[0], 0xC7);
}

TEST(card_getid, fixed_id_and_checksum) {
    MemCard card;
    card.reset();
    std::vector<uint8_t> tx{kSelectCard, CMD_GETID, 0x00, 0x00, 0x00,
                            0, 0, 0, 0, 0, 0};
    const auto rx = transact(card, tx);
    EXPECT_EQ(rx.size(), size_t(11));
    EXPECT_EQ(rx[1], kFlagPre);
    for (int i = 2; i <= 4; ++i) EXPECT_EQ(rx[i], kFlagMid);
    EXPECT_EQ(rx[5], 0x04);
    EXPECT_EQ(rx[6], 0x00);
    EXPECT_EQ(rx[7], 0x00);
    EXPECT_EQ(rx[8], 0x80);
    EXPECT_EQ(rx[9], 0x84);
    EXPECT_EQ(rx[10], kFlagGood);
}

TEST(card_erase, fills_sector_with_ff) {
    MemCard card = pattern_card();
    std::vector<uint8_t> tx{kSelectCard, CMD_ERASE, 0x00, 0x05, 0x00, 0x00};
    const auto rx = transact(card, tx);
    EXPECT_EQ(rx.size(), size_t(6));
    EXPECT_EQ(rx[1], kFlagPre);
    for (int i = 2; i <= 4; ++i) EXPECT_EQ(rx[i], kFlagMid);
    EXPECT_EQ(rx[5], kFlagGood);
    for (unsigned i = 0; i < kSectorSize; ++i) {
        EXPECT_EQ(card.sector_data(5)[i], 0xFF);
    }
}

TEST(card_frame, unknown_command_dead_until_deselect) {
    MemCard card;
    card.reset();
    card.select(true);
    EXPECT_EQ(card.handle(kSelectCard), 0xFF);
    EXPECT_EQ(card.handle(0x51), 0xFF);   // unknown command
    EXPECT_EQ(card.handle(0x00), 0xFF);   // stays dead...
    card.select(false);
    card.select(true);
    EXPECT_EQ(card.handle(kSelectCard), 0xFF);
    EXPECT_EQ(card.handle(CMD_GETID), kFlagPre);  // ...recovers
}

TEST(card_ack, asserted_until_final_flag) {
    MemCard card = pattern_card();
    card.select(true);
    EXPECT_FALSE(card.ack());             // idle: nothing armed
    card.handle(kSelectCard);
    EXPECT_FALSE(card.ack());             // select byte drew 0xFF, still Idle
    card.handle(CMD_READ);
    EXPECT_TRUE(card.ack());              // pre flag drawn, address owed
    card.handle(0x00);                    // addr msb
    card.handle(0x00);                    // addr mid
    card.handle(0x00);                    // addr lsb -> streaming begins
    for (unsigned i = 0; i < kSectorSize; ++i) {
        EXPECT_TRUE(card.ack());          // payload byte coming
        card.handle(0x00);
    }
    EXPECT_TRUE(card.ack());              // checksum byte next
    card.handle(0x00);                    // chk out
    EXPECT_TRUE(card.ack());              // only the end flag remains
    card.handle(0x00);                    // end flag
    EXPECT_FALSE(card.ack());             // transaction complete
}
