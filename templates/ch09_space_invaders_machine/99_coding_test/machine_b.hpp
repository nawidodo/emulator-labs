#pragma once
#include <cstdint>
#include <cstring>
#include <ostream>
#include <string>
#include <vector>

#include "cpu.hpp"
#include "hardware.hpp"
#include "video.hpp"

// Arcade-8080-B — a FICTIONAL 8080 arcade machine used by the coding
// test. Same CPU, different board: everything below is derived from a
// MachineSpec instead of hard-wired constants, which is exactly the skill
// the unseen-spec grader probes (curriculum "coding test" gate).
//
// See CODING_TEST.md for the full board document.

namespace sib {

using si::BusDevice;

struct MachineSpec {
    const char* name;

    uint16_t rom_base;
    uint32_t rom_bytes;        // multiple of 2 KiB bank size
    uint16_t ram_base;
    uint32_t ram_bytes;
    uint16_t vram_base;
    uint32_t vram_bytes;

    uint32_t khz;              // machine clock; frame = khz*1000/60 cycles
    uint8_t irq_opcode_even;   // jammed at even-frame boundaries
    uint8_t irq_opcode_odd;

    uint8_t input_port_lo;     // IN n/n+1/n+2 -> input latches 0/1/2
    uint8_t shifter_read;      // IN port of shifter #1 result
    uint8_t shifter_write;     // OUT port that shifts data into shifter #1
    uint8_t shifter2_read;     // second shifter (the B-board extra)
    uint8_t shifter2_write;
    uint8_t sound_ports[2];    // event-recorder stubs
    uint8_t watchdog_port;
};

constexpr uint32_t cycles_per_frame(uint32_t khz) {
    return khz * 1000u / 60u;
}

// The documented board description for the hidden grader's fixtures.
constexpr MachineSpec kArcade8080B = {
    "Arcade-8080-B",
    0x0000, 0x4000,            // 16 KiB ROM (eight 2 KiB banks)
    0x4000, 0x0400,            // 1 KiB RAM
    0x4400, 0x1C00,            // 7 KiB VRAM
    2160,                      // 2160 kHz -> exactly 36000 cycles/frame
    0xDF, 0xF7,                // RST -> vector 0x18 even, 0x30 odd
    0x00,                      // inputs on IN 0..2
    0x06, 0x06,                // shifter #1 on IN 6 / OUT 6
    0x07, 0x07,                // shifter #2 on IN 7 / OUT 7
    {0x04, 0x05},              // sound stubs on OUT 4 / OUT 5
    0x00                       // watchdog kick on OUT 0
};

// Size-generic byte-window device covering ROM/RAM/VRAM duties on B.
class ByteWindow final : public BusDevice {
public:
    enum class Kind { kRom, kRam, kVram };

    ByteWindow(Kind kind, uint32_t bytes)
        : kind_(kind), bytes_(bytes), data_(new uint8_t[bytes] {}) {}
    ~ByteWindow() override { delete[] data_; }

    void load_linear(const uint8_t* data, size_t len) {
        const size_t n = len < bytes_ ? len : bytes_;
        if (n) std::memcpy(data_, data, n);
        for (size_t i = n; i < bytes_; ++i) data_[i] = 0xFF;  // erased EPROM
    }

    uint8_t read(uint16_t off) const override {
        return off < bytes_ ? data_[off] : uint8_t(0x00);
    }
    void write(uint16_t off, uint8_t val) override {
        if (kind_ == Kind::kRom) return;              // masked ROM
        if (off < bytes_) data_[off] = val;
    }

    const uint8_t* bytes() const { return data_; }
    uint32_t size() const { return bytes_; }

private:
    Kind kind_;
    uint32_t bytes_;
    uint8_t* data_;
};

// The assembled B machine: spec -> bus wiring -> running program.
class MachineB final : public i8080::Bus {
public:
    explicit MachineB(const MachineSpec& spec) : spec_(spec) {
        rom_ = new ByteWindow(ByteWindow::Kind::kRom, spec_.rom_bytes);
        ram_ = new ByteWindow(ByteWindow::Kind::kRam, spec_.ram_bytes);
        vram_ = new ByteWindow(ByteWindow::Kind::kVram, spec_.vram_bytes);

        // ---- memory map ------------------------------------------------
//@LABS-BEGIN 1
//@LABS-SOLUTION
        decoder_.attach(spec_.rom_base,
                        uint16_t(spec_.rom_base + spec_.rom_bytes - 1), rom_);
        decoder_.attach(spec_.ram_base,
                        uint16_t(spec_.ram_base + spec_.ram_bytes - 1), ram_);
        decoder_.attach(spec_.vram_base,
                        uint16_t(spec_.vram_base + spec_.vram_bytes - 1),
                        vram_);
//@LABS-STUB
        // TODO(1): attach the ROM/RAM/VRAM windows from the spec to the
        // decoder. Devices see LOCAL offsets; windows are [base,
        // base+bytes-1].
//@LABS-END

        // ---- timers ----------------------------------------------------
//@LABS-BEGIN 2
//@LABS-SOLUTION
        timers_.configure(cycles_per_frame(spec_.khz),
                          spec_.irq_opcode_even, spec_.irq_opcode_odd);
//@LABS-STUB
        // TODO(2): configure the vblank timers from the spec — frame
        // period khz*1000/60 cycles, even/odd jam opcodes as specified.
//@LABS-END

        io_.attach(&shifter_, &shifter2_, &sound_, &watchdog_);
        cpu_.reset();
        cpu_.bus = this;
    }
    ~MachineB() override { delete rom_; delete ram_; delete vram_; }

    void load_rom(const uint8_t* data, size_t len) {
        rom_->load_linear(data, len);
        cpu_.reset();
        cpu_.bus = this;
    }

    void set_inputs(const si::InputFrame& f) { io_.set_inputs(f); }

    uint8_t read(uint16_t addr) const override { return decoder_.read(addr); }
    void write(uint16_t addr, uint8_t val) override {
        decoder_.write(addr, val);
    }
    uint8_t in(uint8_t port) override { return io_.in(port, spec_); }
    void out(uint8_t port, uint8_t val) override {
        io_.out(port, val, cpu_.cycles, spec_);
    }

    void run(uint64_t cycle_budget, std::ostream* trace) {
        while (!cpu_.halted && cpu_.cycles < cycle_budget) {
            const si::IrqRaise irq = timers_.poll(cpu_.cycles);
            if (irq.raised) cpu_.interrupt(irq.opcode);
            if (trace) *trace << trace_line();
            cpu_.step();
        }
        render();
    }

    void render() { si::render_frame(vram_->bytes(), &frame_); }
    const si::Frame& frame() const { return frame_; }
    uint64_t frame_hash() const { return si::fnv64(frame_.rgba, si::Frame::kBytes); }

    const MachineSpec& spec() const { return spec_; }
    i8080::Cpu& cpu() { return cpu_; }
    ByteWindow& rom() { return *rom_; }
    ByteWindow& ram() { return *ram_; }
    ByteWindow& vram() { return *vram_; }
    si::SoundRecorder& sound() { return sound_; }
    si::Watchdog& watchdog() { return watchdog_; }
    si::VblankTimers& timers() { return timers_; }

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

    std::string state_line() const {
        char buf[160];
        const uint8_t f =
            i8080::pack_psw(cpu_.s, cpu_.z, cpu_.ac, cpu_.p, cpu_.cy);
        std::snprintf(buf, sizeof buf,
                      "AF=%02X%02X BC=%02X%02X DE=%02X%02X HL=%02X%02X "
                      "SP=%04X PC=%04X cyc=%llu",
                      cpu_.a, f, cpu_.b, cpu_.c, cpu_.d, cpu_.e,
                      cpu_.h, cpu_.l, cpu_.sp, cpu_.pc,
                      static_cast<unsigned long long>(cpu_.cycles));
        return std::string(buf);
    }

private:
    // Spec-driven I/O space: two shift registers whose shift amounts come
    // from OUT 2 (bits 0-2 -> shifter #1, bits 3-5 -> shifter #2), two
    // sound-event recorder ports and a kicked watchdog.
    struct Io {
        si::ShiftRegister* sr1 = nullptr;
        si::ShiftRegister* sr2 = nullptr;
        si::SoundRecorder* sound = nullptr;
        si::Watchdog* watchdog = nullptr;
        si::InputFrame inputs{};

        void attach(si::ShiftRegister* a, si::ShiftRegister* b,
                    si::SoundRecorder* s, si::Watchdog* w) {
            sr1 = a; sr2 = b; sound = s; watchdog = w;
        }
        void set_inputs(const si::InputFrame& f) { inputs = f; }

        uint8_t in(uint8_t port, const MachineSpec& sp) const {
//@LABS-BEGIN 3
//@LABS-SOLUTION
            if (port == sp.input_port_lo) return inputs.port0;
            if (port == uint8_t(sp.input_port_lo + 1)) return inputs.port1;
            if (port == uint8_t(sp.input_port_lo + 2)) return inputs.port2;
            if (port == sp.shifter_read && sr1) return sr1->read();
            if (port == sp.shifter2_read && sr2) return sr2->read();
            return 0x00;
//@LABS-STUB
            // TODO(3): decode reads — three input latches at
            // input_port_lo..+2, shifter #1 result at shifter_read,
            // shifter #2 result at shifter2_read; unassigned ports 0x00.
            (void)port;
            (void)sp;
            return 0x00;  // wrong on purpose: everything floats low
//@LABS-END
        }
        void out(uint8_t port, uint8_t val, uint64_t cycle,
                 const MachineSpec& sp) {
//@LABS-BEGIN 4
//@LABS-SOLUTION
            if (port == 0x02) {
                if (sr1) sr1->set_amount(val);
                if (sr2) sr2->set_amount(uint8_t(val >> 3));
            } else if (port == sp.shifter_write && sr1) {
                sr1->write_data(val);
            } else if (port == sp.shifter2_write && sr2) {
                sr2->write_data(val);
            } else if (port == sp.sound_ports[0] ||
                       port == sp.sound_ports[1]) {
                if (sound) sound->record(cycle, port, val);
            } else if (port == sp.watchdog_port) {
                if (watchdog) watchdog->kick(cycle);
                if (sound) sound->record(cycle, port, val);
            }
//@LABS-STUB
            // TODO(4): decode writes — OUT 2 splits its bits between the
            // two shift amounts (0-2 -> #1, 3-5 -> #2); shifter data on
            // the spec's write ports; sound events recorded; watchdog
            // kicked AND recorded. Unassigned ports drop.
            (void)port;
            (void)val;
            (void)cycle;
            (void)sp;
//@LABS-END
        }
    };

    MachineSpec spec_{};
    si::AddressDecoder decoder_;
    ByteWindow* rom_ = nullptr;
    ByteWindow* ram_ = nullptr;
    ByteWindow* vram_ = nullptr;
    Io io_;
    si::ShiftRegister shifter_;
    si::ShiftRegister shifter2_;
    si::SoundRecorder sound_;
    si::Watchdog watchdog_;
    si::VblankTimers timers_;
    i8080::Cpu cpu_;
    si::Frame frame_{};
};

}  // namespace sib
