#pragma once
#include <cstdint>
#include <cstdio>
#include <ostream>
#include <string>
#include <vector>

#include "cpu.hpp"
#include "hardware.hpp"
#include "video.hpp"

// The assembled Space Invaders machine: CPU -> bus -> devices.
//
// The machine itself implements i8080::Bus: memory cycles go through the
// AddressDecoder, I/O cycles through IoPorts. run() advances the CPU one
// instruction at a time, polling the vblank timers before every step so a
// pending interrupt is jammed at exactly the cycle it fires.

namespace si {

class SpaceInvadersMachine final : public i8080::Bus {
public:
    SpaceInvadersMachine() {
        decoder_.attach(kRomBase, uint16_t(kRomBase + kRomSize - 1), &rom_);
        decoder_.attach(kRamBase, uint16_t(kRamBase + kRamSize - 1), &ram_);
        decoder_.attach(kVramBase, uint16_t(kVramBase + kVramSize - 1), &vram_);
        io_.attach(&shifter_, &sound_, &watchdog_);
    }

    // ---- composition (chapter 9 exercises wire these pieces) ----------

    AddressDecoder& decoder() { return decoder_; }
    RomDevice& rom() { return rom_; }
    RamDevice& ram() { return ram_; }
    VramDevice& vram() { return vram_; }
    IoPorts& io() { return io_; }
    ShiftRegister& shifter() { return shifter_; }
    SoundRecorder& sound() { return sound_; }
    Watchdog& watchdog() { return watchdog_; }
    VblankTimers& timers() { return timers_; }
    i8080::Cpu& cpu() { return cpu_; }

    void load_rom(const uint8_t* data, size_t len) {
        // Split the image across the four 2 KiB banks in linear order.
        for (size_t off = 0; off < kRomSize; off += kRomBank) {
            const size_t n = len > off ? ((len - off < kRomBank) ? len - off : kRomBank) : 0;
            rom_.load_bank(int(off / kRomBank), data + off, n);
        }
        cpu_.reset();
        cpu_.bus = this;
    }

    void set_inputs(const InputFrame& f) { io_.set_inputs(f); }

    // ---- i8080::Bus ----------------------------------------------------

    uint8_t read(uint16_t addr) const override { return decoder_.read(addr); }
    void write(uint16_t addr, uint8_t val) override { decoder_.write(addr, val); }
    uint8_t in(uint8_t port) override { return io_.in(port); }
    void out(uint8_t port, uint8_t val) override { io_.out(port, val, cpu_.cycles); }

    // ---- execution -----------------------------------------------------

    // Run until HLT or `cycle_budget` cumulative T-states. Interrupts are
    // delivered at frame boundaries before the instruction that would
    // cross them. When `trace` is non-null, one line per executed
    // instruction is appended (the canonical chapter trace format).
    void run(uint64_t cycle_budget, std::ostream* trace) {
        while (!cpu_.halted && cpu_.cycles < cycle_budget) {
            const IrqRaise irq = timers_.poll(cpu_.cycles);
            if (irq.raised) cpu_.interrupt(irq.opcode);
            if (trace) *trace << trace_line();
            cpu_.step();
        }
        render();
    }

    const Frame& frame() const { return frame_; }

    void render() { render_frame(vram().bytes(), &frame_); }

    uint64_t frame_hash() const {
        return fnv64(frame_.rgba, Frame::kBytes);
    }

    std::string trace_line() const {
        char line[128];
        const uint8_t flags = uint8_t(
            (cpu_.s ? 0x80 : 0) | (cpu_.z ? 0x40 : 0) | (cpu_.ac ? 0x10 : 0) |
            (cpu_.p ? 0x04 : 0) | (cpu_.cy ? 0x01 : 0) | 0x02);
        std::snprintf(line, sizeof line,
                      "pc=%04X op=%02X af=%02X%02X bc=%02X%02X de=%02X%02X "
                      "hl=%02X%02X sp=%04X cyc=%llu\n",
                      cpu_.pc, read(cpu_.pc), cpu_.a, flags,
                      cpu_.b, cpu_.c, cpu_.d, cpu_.e,
                      cpu_.h, cpu_.l, cpu_.sp,
                      static_cast<unsigned long long>(cpu_.cycles));
        return std::string(line);
    }

    // Final state line — same shape as every chapter's runner output.
    std::string state_line() const {
        char buf[160];
        const uint8_t f = i8080::pack_psw(cpu_.s, cpu_.z, cpu_.ac,
                                          cpu_.p, cpu_.cy);
        std::snprintf(buf, sizeof buf,
                      "AF=%02X%02X BC=%02X%02X DE=%02X%02X HL=%02X%02X "
                      "SP=%04X PC=%04X cyc=%llu",
                      cpu_.a, f, cpu_.b, cpu_.c, cpu_.d, cpu_.e,
                      cpu_.h, cpu_.l, cpu_.sp, cpu_.pc,
                      static_cast<unsigned long long>(cpu_.cycles));
        return std::string(buf);
    }

private:
    AddressDecoder decoder_;
    RomDevice rom_;
    RamDevice ram_;
    VramDevice vram_;
    IoPorts io_;
    ShiftRegister shifter_;
    SoundRecorder sound_;
    Watchdog watchdog_;
    VblankTimers timers_;
    i8080::Cpu cpu_;
    Frame frame_{};
};

}  // namespace si
