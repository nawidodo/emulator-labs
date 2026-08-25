#define LABSTEST_MAIN
#include "labstest.hpp"

#include "io_ports.hpp"

#include <string>
#include <vector>

using namespace si;

namespace {

struct Rig {
    ShiftRegister shifter;
    SoundRecorder sound;
    Watchdog watchdog;
    IoPorts io;

    Rig() { io.attach(&shifter, &sound, &watchdog); }
};

}  // namespace

TEST(script, parses_triples_comments_and_blanks) {
    const std::string text =
        "# coin pressed on frame 0\n"
        "01 00 00\n"
        "\n"
        "   00 04 2A   # fire + dips\n"
        "00 02 3C\n";
    std::vector<InputFrame> frames;
    EXPECT_TRUE(parse_input_script(text, &frames));
    EXPECT_EQ(frames.size(), 3u);
    EXPECT_EQ(frames[0].port0, 0x01);
    EXPECT_EQ(frames[1].port1, 0x04);
    EXPECT_EQ(frames[1].port2, 0x2A);
    EXPECT_EQ(frames[2].port2, 0x3C);
}

TEST(script, partial_line_is_an_error) {
    std::vector<InputFrame> frames;
    EXPECT_FALSE(parse_input_script("01 02\n", &frames));   // missing P2
    EXPECT_FALSE(parse_input_script("05\n", &frames));      // only P0

    // Two tokens is also malformed (P0 P1 without P2).
    EXPECT_FALSE(parse_input_script("01 02 \n", &frames));
}

TEST(script, empty_script_yields_no_frames) {
    std::vector<InputFrame> frames;
    EXPECT_TRUE(parse_input_script("", &frames));
    EXPECT_TRUE(frames.empty());
    EXPECT_TRUE(parse_input_script("# only a comment\n\n", &frames));
    EXPECT_TRUE(frames.empty());
}

TEST(io, input_latches_reach_their_ports) {
    Rig r;
    r.io.set_inputs({0x11, 0x22, 0x33});
    EXPECT_EQ(r.io.in(0), 0x11);
    EXPECT_EQ(r.io.in(1), 0x22);
    EXPECT_EQ(r.io.in(2), 0x33);
    EXPECT_EQ(r.io.in(4), 0x00);   // unassigned reads float low
}

TEST(io, out2_latches_shift_amount_for_in3) {
    Rig r;
    r.io.out(2, 0xF3, 10);         // upper bits ignored -> amount 3
    r.shifter.write_data(0xDE);
    r.shifter.write_data(0xAD);    // raw = 0xADDE
    EXPECT_EQ(r.io.in(3), 0xBB);   // (0xADDE >> 3) & 0xFF
}

TEST(io, out4_writes_flow_to_in3_with_amount_zero) {
    Rig r;
    r.io.out(4, 0x5A, 0);
    r.io.out(4, 0xC3, 5);
    r.io.out(2, 0x00, 6);
    EXPECT_EQ(r.io.in(3), 0x5A);   // amount 0 exposes the LOW (first) byte
}

TEST(io, sound_ports_record_events_but_change_nothing_else) {
    Rig r;
    r.io.out(3, 0x55, 100);
    r.io.out(5, 0xAA, 250);
    const auto& ev = r.sound.events();
    EXPECT_EQ(ev.size(), 2u);
    EXPECT_EQ(ev[0].cycle, 100u);
    EXPECT_EQ(ev[0].port, 3);
    EXPECT_EQ(ev[0].value, 0x55);
    EXPECT_EQ(ev[1].cycle, 250u);
    EXPECT_EQ(ev[1].port, 5);
    EXPECT_EQ(ev[1].value, 0xAA);

    // Sound writes leave no other trace: no latch changes, no shift.
    EXPECT_EQ(r.io.in(0), 0x00);
    EXPECT_EQ(r.io.in(3), 0x00);
}

TEST(io, watchdog_kick_is_recorded_and_timestamped) {
    Rig r;
    r.io.out(6, 0x01, 500);
    EXPECT_TRUE(r.watchdog.kicked());
    EXPECT_EQ(r.watchdog.last_kick(), 500u);
    EXPECT_FALSE(r.watchdog.expired(500 + 100, 200));

    // Port 6 events land in the log too (observability beats silence).
    bool saw_port6 = false;
    for (const auto& e : r.sound.events())
        if (e.port == 6 && e.value == 0x01 && e.cycle == 500)
            saw_port6 = true;
    EXPECT_TRUE(saw_port6);
}

TEST(io, unassigned_out_ports_drop_silently) {
    Rig r;
    r.io.out(7, 0xFF, 1);          // no port 7 on this board
    r.io.out(1, 0xFF, 2);          // IN 1's latch is not writable via OUT 1
    EXPECT_TRUE(r.sound.events().empty());
    EXPECT_EQ(r.io.in(1), 0x00);
}
