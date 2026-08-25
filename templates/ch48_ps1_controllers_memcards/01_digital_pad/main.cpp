#define LABSTEST_MAIN
#include "labstest.hpp"
#include "pad.hpp"

using namespace sio;

// A "nothing pressed" reference report.
static Buttons none() { return {}; }

TEST(pad_pack, empty_is_zero) {
    EXPECT_EQ(pack_buttons(none()), 0u);
}

TEST(pad_pack, individual_bits_match_layout) {
    Buttons b;
    b.cross = true;
    EXPECT_EQ(pack_buttons(b), BTN_CROSS);
    b = none();
    b.start = true;
    b.l1 = true;
    EXPECT_EQ(pack_buttons(b), static_cast<uint16_t>(BTN_START | BTN_L1));
    b = none();
    b.square = true;
    EXPECT_EQ(pack_buttons(b), BTN_SQUARE);
}

TEST(pad_pack, all_pressed_is_all_bits) {
    Buttons b;
    b.select = b.start = b.up = b.right = b.down = b.left = true;
    b.l2 = b.r2 = b.l1 = b.r1 = true;
    b.triangle = b.circle = b.cross = b.square = true;
    const uint16_t expect = BTN_SELECT | BTN_START | BTN_UP | BTN_RIGHT |
                            BTN_DOWN | BTN_LEFT | BTN_L2 | BTN_R2 | BTN_L1 |
                            BTN_R1 | BTN_TRIANGLE | BTN_CIRCLE | BTN_CROSS |
                            BTN_SQUARE;
    EXPECT_EQ(pack_buttons(b), expect);
}

TEST(pad_report, active_low_inversion) {
    Buttons b;
    b.circle = true;
    // circle is bit 13: pressing it CLEARS that bit of the wire word.
    EXPECT_EQ(report_word(b), static_cast<uint16_t>(~BTN_CIRCLE));
}

TEST(pad_report, nothing_pressed_reads_ffff) {
    EXPECT_EQ(report_word(none()), 0xFFFF);
}

TEST(pad_report, l3_r3_always_released_on_digital) {
    Buttons b;
    b.l3 = b.r3 = true;  // a digital pad has no clickable sticks
    EXPECT_EQ(report_word(b) & (BTN_L3 | BTN_R3), BTN_L3 | BTN_R3);
}

TEST(pad_report, roundtrip_through_buttons_from_report) {
    Buttons b;
    b.up = b.cross = b.r2 = true;
    const Buttons back = buttons_from_report(report_word(b));
    EXPECT_TRUE(back.up && back.cross && back.r2);
    EXPECT_FALSE(back.start || back.left || b.square);
}

// Full wire session: tx 01 42 00 00 00 00 -> rx FF FF 41 5A lo hi.
TEST(pad_read, id_and_button_sequence) {
    DigitalPad pad;
    Buttons b;
    b.left = true;
    pad.set_buttons(b);
    pad.select(true);
    const uint8_t tx[] = {kSelectPad, kPadCmdRead, 0, 0, 0, 0};
    const uint8_t want[] = {0xFF, 0xFF, kPadIdLo, kPadIdHi,
                            0x7F,     // left = bit 7 -> low byte clears bit 7
                            static_cast<uint8_t>(report_word(b) >> 8)};
    for (int i = 0; i < 6; ++i) {
        EXPECT_EQ(pad.handle(tx[i]), want[i]);
    }
}

TEST(pad_read, unselected_returns_ff) {
    DigitalPad pad;
    EXPECT_EQ(pad.handle(kSelectPad), 0xFF);
    EXPECT_EQ(pad.handle(kPadCmdRead), 0xFF);
}

TEST(pad_read, unknown_command_ff_until_deselect) {
    DigitalPad pad;
    pad.set_buttons(none());
    pad.select(true);
    EXPECT_EQ(pad.handle(kSelectPad), 0xFF);
    EXPECT_EQ(pad.handle(0x44), 0xFF);   // not a pad command
    EXPECT_EQ(pad.handle(0x00), 0xFF);   // stays mute...
    EXPECT_EQ(pad.handle(0x00), 0xFF);
    pad.select(false);
    pad.select(true);
    EXPECT_EQ(pad.handle(kSelectPad), 0xFF);
    EXPECT_EQ(pad.handle(kPadCmdRead), 0xFF);
    EXPECT_EQ(pad.handle(0x00), kPadIdLo);  // ...but recovers after deselect
}

TEST(pad_read, buttons_low_byte_first) {
    DigitalPad pad;
    Buttons b;
    b.triangle = true;  // bit 12 -> high byte bit 4 cleared
    pad.set_buttons(b);
    pad.select(true);
    pad.handle(kSelectPad);
    pad.handle(kPadCmdRead);
    EXPECT_EQ(pad.handle(0x00), kPadIdLo);
    EXPECT_EQ(pad.handle(0x00), kPadIdHi);
    EXPECT_EQ(pad.handle(0x00), 0xFF);  // low byte: only high-byte bits used
    EXPECT_EQ(pad.handle(0x00), static_cast<uint8_t>(report_word(b) >> 8));
}

TEST(pad_ack, pulses_between_data_bytes_not_after_final) {
    DigitalPad pad;
    pad.set_buttons(none());
    pad.select(true);
    EXPECT_FALSE(pad.ack());               // nothing armed yet
    pad.handle(kSelectPad);
    EXPECT_FALSE(pad.ack());
    pad.handle(kPadCmdRead);
    EXPECT_FALSE(pad.ack());               // no response bytes emitted yet
    pad.handle(0x00);                      // rx 41
    EXPECT_TRUE(pad.ack());                // more data owed
    pad.handle(0x00);                      // rx 5A
    EXPECT_TRUE(pad.ack());
    pad.handle(0x00);                      // rx buttons lo
    EXPECT_TRUE(pad.ack());
    pad.handle(0x00);                      // rx buttons hi — final
    EXPECT_FALSE(pad.ack());
}

TEST(pad_ack, silent_outside_session) {
    DigitalPad pad;
    pad.set_buttons(none());
    EXPECT_FALSE(pad.ack());
    pad.select(true);
    pad.handle(kSelectPad);
    pad.handle(0x44);  // unknown command: session never arms
    EXPECT_FALSE(pad.ack());
}

TEST(pad_select, rising_edge_resets_mid_transaction) {
    DigitalPad pad;
    pad.set_buttons(none());
    pad.select(true);
    pad.handle(kSelectPad);
    pad.handle(kPadCmdRead);
    pad.handle(0x00);                      // rx 41, count_=1
    pad.select(false);                     // host aborts mid-read
    pad.select(true);
    pad.handle(kSelectPad);
    EXPECT_EQ(pad.handle(kPadCmdRead), 0xFF);
    EXPECT_EQ(pad.handle(0x00), kPadIdLo); // fresh session starts at the ID
}
