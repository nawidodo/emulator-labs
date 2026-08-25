#define LABSTEST_MAIN
#include "labstest.hpp"
#include "machine.hpp"

#include <array>

namespace {

// Draws an 8x1 row at (0,0), then spins forever.
const std::array<uint8_t, 12> kTinyRom = {
    0xA2, 0x0A,        // LD I, 0x20A
    0x60, 0x00,        // LD V0, 0
    0x61, 0x00,        // LD V1, 0
    0xD0, 0x11,        // DRW V0, V1, 1
    0x12, 0x08,        // JP 0x208
    0xFF, 0xFF,        // sprite rows
};

chip8::Machine rom_machine() {
    chip8::Machine m;
    m.load(kTinyRom);
    return m;
}

}  // namespace

TEST(machine, cls_clears_display) {
    chip8::Machine m;
    m.display.set(5, 5, true);
    m.mem[0x200] = 0x00;
    m.mem[0x201] = 0xE0;
    m.step();
    EXPECT_FALSE(m.display.get(5, 5));
}

TEST(machine, dxyn_sets_collision_flag_in_vf) {
    chip8::Machine m = rom_machine();
    m.step();              // LD I, 0x20A
    EXPECT_EQ(m.i, 0x20A);
    m.v[0] = 0;
    m.v[1] = 0;
    m.pc = 0x206;          // DRW V0, V1, 1
    m.step();
    EXPECT_EQ(m.v[0xF], 0);
    EXPECT_TRUE(m.display.get(0, 0));
    m.pc -= 2;             // draw again over the same spot
    m.step();
    EXPECT_EQ(m.v[0xF], 1);             // collision
    EXPECT_FALSE(m.display.get(0, 0));  // erased
}

TEST(machine, key_skip_e_and_a1) {
    chip8::Machine m;
    m.v[3] = 7;

    // EX9E with key held: skips the next instruction.
    m.mem[m.pc] = 0xE3; m.mem[m.pc + 1] = 0x9E;
    m.keypad.press(7);
    m.step();
    EXPECT_EQ(m.pc, chip8::kProgStart + 4);

    // EX9E without the key: falls through.
    m.mem[m.pc] = 0xE3; m.mem[m.pc + 1] = 0x9E;
    m.keypad.release(7);
    m.step();
    EXPECT_EQ(m.pc, chip8::kProgStart + 6);

    // EXA1 without the key: skips.
    m.mem[m.pc] = 0xE3; m.mem[m.pc + 1] = 0xA1;
    m.step();
    EXPECT_EQ(m.pc, chip8::kProgStart + 10);
}

TEST(machine, timer_moves_fx07_fx15_fx18) {
    chip8::Machine m;
    m.timers.set_delay(42);

    // FX07: V0 = DT. (There is no opcode that READS ST — it is audible
    // only — so we check sound via timers.sound after FX18.)
    m.mem[m.pc] = 0xF0; m.mem[m.pc + 1] = 0x07;
    m.step();
    EXPECT_EQ(m.v[0], 42);

    m.v[2] = 30;
    m.mem[m.pc] = 0xF2; m.mem[m.pc + 1] = 0x15;   // DT = V2
    m.step();
    EXPECT_EQ(m.timers.delay, 30);

    m.mem[m.pc] = 0xF2; m.mem[m.pc + 1] = 0x18;   // ST = V2
    m.step();
    EXPECT_EQ(m.timers.sound, 30);
    EXPECT_TRUE(m.timers.beeping());
}

TEST(machine, fx0a_blocks_until_key_then_stores_it) {
    chip8::Machine m;
    // FX0A at 0x200: no key held -> pc stays on the instruction.
    m.mem[0x200] = 0xF0; m.mem[0x201] = 0x0A;
    m.step();
    EXPECT_EQ(m.pc, 0x200);       // blocked: retrying same opcode
    EXPECT_EQ(m.v[0], 0);

    m.keypad.press(0xB);
    m.step();
    EXPECT_EQ(m.pc, 0x202);       // released from the wait
    EXPECT_EQ(m.v[0], 0xB);
}


TEST(run_driver, run_executes_exactly_n_cycles) {
    chip8::Machine m = rom_machine();
    const uint16_t start_pc = m.pc;
    m.run(3);
    // 3 instructions: LD I, LD V0, LD V1.
    EXPECT_EQ(m.pc, static_cast<int>(start_pc + 6));
}

TEST(run_driver, run_returns_frames_advanced_per_call) {
    chip8::Machine m = rom_machine();
    EXPECT_EQ(m.run(9), 0u);      // boundary: 1 short of a frame
    EXPECT_EQ(m.run(11), 1u);     // this call alone spans one boundary
}

TEST(run_driver, frame_count_boundaries) {
    {
        chip8::Machine m = rom_machine();
        EXPECT_EQ(m.run(10), 1u);   // exactly one frame
    }
    {
        chip8::Machine m = rom_machine();
        EXPECT_EQ(m.run(35), 3u);   // three whole frames out of 35 cycles
    }
    {
        chip8::Machine m = rom_machine();
        EXPECT_EQ(m.run(0), 0u);    // nothing at all
    }
}

TEST(run_driver, timers_tick_every_ten_cycles_during_run) {
    chip8::Machine m = rom_machine();
    m.timers.set_delay(100);
    m.run(25);                    // two full tick windows + 5 spare cycles
    EXPECT_EQ(m.timers.delay, 98);
    m.run(5);                     // banks the spare cycles -> third tick
    EXPECT_EQ(m.timers.delay, 97);
}

TEST(run_driver, beep_recorded_across_run_frames) {
    chip8::Machine m = rom_machine();
    std::vector<bool> events;
    m.timers.on_beep = [&](bool started) { events.push_back(started); };
    m.timers.set_sound(3);        // 3 ticks long
    m.run(35);                    // three ticks -> beep ended
    EXPECT_TRUE(events.size() >= static_cast<std::size_t>(2));
    EXPECT_TRUE(events.front());
    EXPECT_FALSE(events.back());
}


TEST(run_for, zero_cycles_zero_ticks_is_noop) {
    chip8::Machine m = rom_machine();
    const uint16_t pc0 = m.pc;
    m.timers.set_delay(7);
    m.run_for(0, 0);
    EXPECT_EQ(m.pc, pc0);
    EXPECT_EQ(m.timers.delay, 7);
    EXPECT_EQ(m.v[0], 0);
}

TEST(run_for, ticks_without_cycles) {
    chip8::Machine m = rom_machine();
    m.timers.set_delay(5);
    m.run_for(0, 5);              // pure timer time: no CPU progress
    EXPECT_EQ(m.pc, chip8::kProgStart);
    EXPECT_EQ(m.timers.delay, 0);
    EXPECT_EQ(m.v[0], 0);
}

TEST(run_for, cycles_without_ticks) {
    chip8::Machine m = rom_machine();
    m.timers.set_delay(5);
    m.run_for(50, 0);             // five frames of CPU, zero timer time
    EXPECT_EQ(m.timers.delay, 5);
    EXPECT_NE(m.pc, chip8::kProgStart);
}

TEST(run_for, matches_run_when_rates_align) {
    // run_for(cycles, ticks) must agree with run() whenever ticks ==
    // cycles / kCyclesPerTimerTick — the decoupled API subsumes the coupled
    // one.
    chip8::Machine a = rom_machine();
    a.timers.set_delay(50);
    a.run_for(23, 2);

    chip8::Machine b = rom_machine();
    b.timers.set_delay(50);
    b.run(23);                    // 2 whole tick windows, remainder banked
    EXPECT_EQ(a.timers.delay, b.timers.delay);
    EXPECT_EQ(a.pc, b.pc);
    for (int k = 0; k < 16; ++k) EXPECT_EQ(a.v[k], b.v[k]);
}

TEST(run_for, more_ticks_than_cycles_allow) {
    // Timer time may outrun CPU time arbitrarily: that is the point of the
    // decoupled contract.
    chip8::Machine m = rom_machine();
    m.timers.set_sound(200);
    m.run_for(1, 200);
    EXPECT_EQ(m.timers.sound, 0);
    EXPECT_EQ(m.pc, static_cast<int>(chip8::kProgStart + 2));
}
