// Exercise 13.04 tests -- the full delivery loop: timer raises IF, the
// copied CPU + IntCtl dispatch, priority arbitration, HALT wake rules.
#define LABSTEST_MAIN
#include <array>
#include <cstddef>
#include <cstdint>

#include "labstest.hpp"
#include "machine.hpp"

namespace {

constexpr size_t kImgSize = 0x120;

struct Program {
    std::array<uint8_t, kImgSize> img{};
};

// $0050 ISR : push af; ld a,($FF80); inc a; ld ($FF80),a; pop af; reti
// $0100     : ei; nop; halt
Program make_timer_isr_program() {
    Program p;
    const uint8_t isr[] = {0xF5,                   // push af
                           0xFA, 0x80, 0xFF,       // ld a,($FF80)
                           0x3C,                   // inc a
                           0xEA, 0x80, 0xFF,       // ld ($FF80),a
                           0xF1,                   // pop af
                           0xD9};                  // reti
    for (size_t i = 0; i < sizeof(isr); ++i) p.img[0x50 + i] = isr[i];
    const uint8_t main_[] = {0xFB, 0x00, 0x76};    // ei; nop; halt
    for (size_t i = 0; i < sizeof(main_); ++i) p.img[0x100 + i] = main_[i];
    p.img[0x103] = 0x76;  // second HALT: RETI can land here and re-park
    return p;
}

// Arm an overflow one instruction boundary away: select 01 (DIV bit 3),
// TIMA $FF, tapped bit high and about to fall. TMA reloads $00, so the
// NEXT overflow is a full 256 falls (=4096 T-cycles) away -- tests can
// pick budgets that see exactly one or exactly two events.
void arm_immediate_overflow(gb::TimerMachine& m) {
    m.timer.write_tac(0x05);
    m.timer.tima = 0xFF;
    m.timer.div.counter = 0x0018;
}

}  // namespace

TEST(irq, timer_overflow_dispatches_to_vector_50) {
    gb::TimerMachine m;
    m.load(make_timer_isr_program().img);
    m.ctl.enabled = 0x04;  // IE bit 2: timer
    arm_immediate_overflow(m);

    m.run(200);

    EXPECT_EQ(m.ram.mem[0xFF80], 1);    // ISR ran exactly once
    EXPECT_EQ(m.cpu.pc, 0x0103);        // RETI landed on the HALT; it retired
    EXPECT_TRUE(m.cpu.halted);          // asleep again
    EXPECT_TRUE(m.ctl.ime);             // RETI restored IME
    EXPECT_EQ(m.ctl.flags & 0x04, 0);   // IF bit consumed by the dispatch
}

TEST(irq, periodic_overflows_service_repeatedly) {
    gb::TimerMachine m;
    m.load(make_timer_isr_program().img);
    m.ctl.enabled = 0x04;
    arm_immediate_overflow(m);
    m.run(5000);  // next overflow arrives one full TIMA period later (~4096)
    EXPECT_EQ(m.ram.mem[0xFF80], 2);
}

TEST(irq, priority_vblank_beats_pending_timer) {
    gb::TimerMachine m;
    m.load(make_timer_isr_program().img);
    m.ctl.ime = true;
    m.ctl.enabled = 0xFF;
    m.ctl.flags = 0x05;  // VBlank AND Timer pending

    const int cycles = gb::service_interrupt(m.cpu, m.ctl);

    EXPECT_EQ(cycles, 20);
    EXPECT_EQ(m.cpu.pc, 0x40);          // VBlank wins
    EXPECT_EQ(m.ctl.flags & 0x01, 0);   // its IF bit cleared...
    EXPECT_EQ(m.ctl.flags & 0x04, 4);   // ...timer still waiting
}

TEST(irq, ime0_wakes_halt_without_servicing) {
    gb::TimerMachine m;
    static const uint8_t prog[] = {
        0xFB,              // @100: ei
        0xF3,              // @101: di (cancels the delayed EI)
        0xEA, 0x0F, 0xFF,  // @102: ld ($FF0F),a -- boot A=$01 -> VBlank
        0xEA, 0xFF, 0xFF,  // @105: ld ($FFFF),a : enable VBlank
        0x76,              // @108: halt (IME off)
        0xEA, 0x00, 0xC0,  // @109: wake lands here; marker store
        0x18, 0xFE,        // @10C: jr -2 (park)
    };
    std::array<uint8_t, sizeof(prog)> img{};
    for (size_t i = 0; i < sizeof(prog); ++i) img[i] = prog[i];
    m.load(img, 0x0100);
    m.cpu.a = 0x01;

    m.run(500);

    EXPECT_EQ(m.ram.mem[0xC000], 0x01);  // woke up and kept running
    EXPECT_FALSE(m.ctl.ime);             // never dispatched
    EXPECT_NE(m.ctl.flags & 0x01, 0);    // IF bit NOT cleared
}
