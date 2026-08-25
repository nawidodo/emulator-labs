#define LABSTEST_MAIN
#include <array>
#include <span>

#include "labstest.hpp"
#include "../01_daa_rotates/bus.hpp"
#include "../01_daa_rotates/core.hpp"
#include "int_ctl.hpp"
#include "../01_daa_rotates/daa_ops.hpp"

namespace {

struct Machine {
    gb::FlatBus ram;
    gb::IntCtl ctl;
    gb::IntBus bus{ram, ctl};
    gb::Cpu cpu;
    gb::IrqHook hook;

    void load(std::span<const uint8_t> program, uint16_t base = 0x0100) {
        ram.mem.fill(0);
        ram.load(program, base);  // vector-page images pass 0x0000
        cpu = gb::Cpu{};
        cpu.bus = &bus;
        gb::install_daa_hook(cpu);
        gb::install_stack_hook(cpu);
        hook.ctl = &ctl;
        hook.install(cpu);
    }

    void run(int max_steps = 100000) {
        for (int i = 0; i < max_steps; ++i) {
            if ((cpu.halted && ctl.pending() == 0) || cpu.trap) break;
            if (cpu.halted && ctl.pending() != 0) cpu.halted = false;
            gb::step_irq(cpu, ctl);
        }
    }
};

}  // namespace

// Builds the canonical EI/HALT/ISR demo image:
//   $0050 ISR: inc a ; reti
//   $0100: ld a,4 / store IF / store IE / ei / nop / halt
// Returns the final halt instruction's expected post-state data.
constexpr size_t kImgSize = 0x120;

struct Program {
    std::array<uint8_t, kImgSize> img{};
    uint16_t halt_pc = 0;  // PC after HALT retires
    uint16_t resume_pc = 0;  // return address pushed by the interrupt
};

Program make_ei_halt_program() {
    Program p;
    p.img[0x50] = 0x3C;  // inc a
    p.img[0x51] = 0xD9;  // reti
    size_t pc = 0x0100;
    auto emit = [&p, &pc](std::initializer_list<uint8_t> bs) {
        for (uint8_t b : bs) p.img[pc++] = b;
    };
    emit({0x3E, 0x04});         // ld a,4
    emit({0xEA, 0x0F, 0xFF});   // ld ($FF0F),a : raise timer line
    emit({0xEA, 0xFF, 0xFF});   // ld ($FFFF),a : enable timer line
    emit({0xFB});               // ei
    emit({0x00});               // nop -- EI lands right after this
    p.resume_pc = static_cast<uint16_t>(pc);
    emit({0x76});               // halt
    p.halt_pc = static_cast<uint16_t>(pc);
    return p;
}

TEST(irq, ei_delay_skips_one_instruction) {
    Machine m;
    const uint8_t prog[] = {0xFB, 0x00};  // ei ; nop
    m.load(prog);
    m.ctl.flags = 0xFF;
    m.ctl.enabled = 0x00;   // nothing enabled: no dispatch side effects
    EXPECT_FALSE(m.ctl.ime);

    m.cpu.step();           // execute EI itself
    gb::service_interrupt(m.cpu, m.ctl);
    EXPECT_FALSE(m.ctl.ime);                // delay has NOT elapsed

    m.cpu.step();           // the following NOP
    gb::service_interrupt(m.cpu, m.ctl);
    EXPECT_TRUE(m.ctl.ime);                 // landed exactly one instr later
}

TEST(irq, di_is_immediate) {
    Machine m;
    const uint8_t prog[] = {0xF3, 0x76};  // di; halt
    m.load(prog);
    m.ctl.ime = true;
    m.cpu.step();  // DI
    EXPECT_FALSE(m.ctl.ime);
}

TEST(irq, halt_wakes_and_services_timer_vector) {
    Machine m;
    const Program prog = make_ei_halt_program();
    m.load(prog.img, 0x0000);  // image spans $0050 ISR + $0100 main
    m.run();

    EXPECT_EQ(m.cpu.a, 5);              // ISR incremented the ld a,4 value
    EXPECT_EQ(m.ctl.flags & 0x04, 0);   // IF bit cleared by the service
    EXPECT_TRUE(m.cpu.halted);          // back asleep in the second HALT
    EXPECT_EQ(m.ctl.pending(), 0);
}

TEST(irq, priority_order_vblank_first) {
    Machine m;
    const uint8_t prog[] = {0x76};
    m.load(prog);
    m.ctl.ime = true;
    m.ctl.enabled = 0xFF;
    m.ctl.flags = 0x03;  // VBlank AND STAT pending
    const int cycles = gb::service_interrupt(m.cpu, m.ctl);
    EXPECT_EQ(cycles, 20);
    EXPECT_EQ(m.cpu.pc, 0x40);          // VBlank wins
    EXPECT_EQ(m.ctl.flags & 0x01, 0);   // its IF bit cleared
    EXPECT_EQ(m.ctl.flags & 0x02, 0x02);// STAT still pending
    EXPECT_FALSE(m.ctl.ime);
}

TEST(irq, ime_blocks_dispatch_but_not_wake) {
    Machine m;
    // IME clear: HALT still wakes (pending != 0), but no vector taken and
    // the IF bit stays set. The program parks itself in a tight JR loop
    // after storing a marker so the end state is deterministic.
    const uint8_t prog[] = {
        0xFB,              // @100: ei
        0xF3,              // @101: di (cancels the delayed EI)
        0xEA, 0x0F, 0xFF,  // @102: ld ($FF0F),a -- boot A=$01 -> VBlank
        0xEA, 0xFF, 0xFF,  // @105: ld ($FFFF),a : enable VBlank line
        0x76,              // @108: halt (IME off)
        0x00,              // @109: wake lands here, no service
        0xEA, 0x00, 0xC0,  // @10A: ld ($C000),a : marker store
        0x18, 0xFE,        // @10D: jr -2 (park here)
    };
    m.load(prog);
    m.cpu.a = 0x01;
    m.run();
    EXPECT_EQ(m.ram.mem[0xC000], 0x01);  // woke up and kept running
    EXPECT_FALSE(m.ctl.ime);             // never dispatched
    EXPECT_NE(m.ctl.flags & 0x01, 0);    // IF bit NOT cleared
}
