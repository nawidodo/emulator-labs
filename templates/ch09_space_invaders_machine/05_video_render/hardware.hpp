#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

#include "cpu.hpp"

// Space Invaders board hardware, piece by piece.
//
// Everything here is independently instantiable (curriculum §57): a test
// can construct a ShiftRegister or a Watchdog without building the whole
// cabinet. The chapter exercises wrap these exact classes.

namespace si {

// ---------------------------------------------------------------------------
// Board constants — the documented timing/memory model for this chapter.
// ---------------------------------------------------------------------------

constexpr int kScreenWidth   = 224;                 // upright orientation
constexpr int kScreenHeight = 256;

// Fixed machine clock: 1920 kHz. Chosen as 32000 T-states per frame at
// exactly 60 Hz (1920000 / 60) so frame boundaries land on clean cycle
// counts. Historical boards ran ~2 MHz with sloppy vertical timing; we
// trade authenticity for determinism (documented simplification).
constexpr uint32_t kClockKHz = 1920;
constexpr uint64_t kCyclesPerFrame = kClockKHz * 1000ull / 60ull;  // 32000

constexpr uint16_t kRomBase  = 0x0000;
constexpr size_t   kRomSize  = 0x2000;   // 8 KiB = four 2 KiB banks
constexpr size_t   kRomBank  = 0x0800;
constexpr uint16_t kRamBase  = 0x2000;
constexpr size_t   kRamSize  = 0x0400;   // 1 KiB
constexpr uint16_t kVramBase = 0x2400;
constexpr size_t   kVramSize = 0x1C00;   // 7 KiB framebuffer

// Interrupt opcodes jammed onto the bus by the dual one-shot vblank
// timers: RST 08 on even frames, RST 10 on odd frames.
constexpr uint8_t kIrqOpcodeEven = 0xCF;   // RST 08 -> vector 0x0008
constexpr uint8_t kIrqOpcodeOdd  = 0xD7;   // RST 10 -> vector 0x0010

// ---------------------------------------------------------------------------
// Memory-mapped devices
// ---------------------------------------------------------------------------

class BusDevice {
public:
    virtual ~BusDevice() = default;
    virtual uint8_t read(uint16_t addr) const = 0;
    virtual void write(uint16_t addr, uint8_t val) = 0;
};

// 8 KiB of masked ROM made of four independent 2 KiB banks. Writes are
// silently ignored — there is no bank-select latch on this board, banks
// sit linearly at 0000-1FFF and nothing aliases anywhere else.
class RomDevice final : public BusDevice {
public:
    // Fill one 2 KiB bank (0..3). Bytes beyond `len` stay erased (0xFF,
    // like an unprogrammed EPROM position).
    void load_bank(int bank, const uint8_t* data, size_t len) {
        const size_t off = static_cast<size_t>(bank) * kRomBank;
        const size_t n = len < kRomBank ? len : kRomBank;
        if (n) std::memcpy(&rom_[off], data, n);
        for (size_t i = n; i < kRomBank; ++i) rom_[off + i] = 0xFF;
    }

    uint8_t read(uint16_t addr) const override {
        return rom_[addr & (kRomSize - 1)];
    }
    void write(uint16_t, uint8_t) override {}   // masked ROM absorbs writes

private:
    uint8_t rom_[kRomSize] = {};
};

class RamDevice final : public BusDevice {
public:
    static constexpr size_t kSize = kRamSize;

    uint8_t read(uint16_t addr) const override {
        return addr < kSize ? ram_[addr] : uint8_t(0x00);
    }
    void write(uint16_t addr, uint8_t val) override {
        if (addr < kSize) ram_[addr] = val;
    }

    uint8_t* bytes() { return ram_; }
    const uint8_t* bytes() const { return ram_; }

private:
    uint8_t ram_[kSize] = {};
};

// 7 KiB of 1bpp framebuffer memory, column-major: byte (col*32 + y/8),
// bit y%8 is pixel (col, y). The rotation lives HERE — naive row-major
// decoding transposes the image (see 90_debug).
class VramDevice final : public BusDevice {
public:
    static constexpr size_t kSize = kVramSize;

    uint8_t read(uint16_t addr) const override {
        return addr < kSize ? vram_[addr] : uint8_t(0x00);
    }
    void write(uint16_t addr, uint8_t val) override {
        if (addr < kSize) vram_[addr] = val;
    }

    const uint8_t* bytes() const { return vram_; }

private:
    uint8_t vram_[kSize] = {};
};

// Range router: devices attach to disjoint address windows; the first
// matching window wins. Unmapped reads float low (0x00) and unmapped
// writes drop — both documented stand-ins for floating-bus behavior.
class AddressDecoder final : public i8080::Bus {
public:
    struct Window {
        uint16_t lo, hi;
        BusDevice* dev;
    };

    void attach(uint16_t lo, uint16_t hi, BusDevice* dev) {
        map_.push_back({lo, hi, dev});
    }
    const std::vector<Window>& map() const { return map_; }

    uint8_t read(uint16_t addr) const override {
        for (const Window& w : map_)
            if (addr >= w.lo && addr <= w.hi) return w.dev->read(uint16_t(addr - w.lo));
        return 0x00;
    }
    void write(uint16_t addr, uint8_t val) override {
        for (const Window& w : map_)
            if (addr >= w.lo && addr <= w.hi) {
                w.dev->write(uint16_t(addr - w.lo), val);
                return;
            }
    }

private:
    std::vector<Window> map_;
};

// ---------------------------------------------------------------------------
// Port-mapped peripherals
// ---------------------------------------------------------------------------

// The 8-bit hardware shifter behind ports 2/3/4.
//
//   OUT 2 : bits 0-2 latch the shift amount. The counter is 3 bits wide,
//           so amounts wrap modulo 8 exactly like the TTL counter did.
//   OUT 4 : shifts the whole 16-bit register right one BYTE and drops the
//           written byte into the high half. Two successive writes fill
//           LOW byte first, then HIGH ("write-low/high" filling).
//   IN 3  : returns bits [amount .. amount+7] of the 16-bit register.
//
class ShiftRegister {
public:
    void write_data(uint8_t v) { sr_ = uint16_t((sr_ >> 8) | (uint16_t(v) << 8)); }

    void set_amount(uint8_t amt) { amount_ = amt & 0x07; }
    uint8_t amount() const { return amount_; }

    uint8_t read() const { return uint8_t(sr_ >> amount_); }
    uint16_t raw() const { return sr_; }

private:
    uint16_t sr_ = 0;
    uint8_t amount_ = 0;
};

// Sound ports 3/5/6 are documented stubs on this board: they log events
// instead of synthesizing audio. The log IS the observable contract.
struct SoundEvent {
    uint64_t cycle;
    uint8_t port;
    uint8_t value;
};

class SoundRecorder {
public:
    void record(uint64_t cycle, uint8_t port, uint8_t value) {
        events_.push_back({cycle, port, value});
    }
    const std::vector<SoundEvent>& events() const { return events_; }
    void clear() { events_.clear(); }

private:
    std::vector<SoundEvent> events_;
};

// OUT 6 kicks the watchdog. The documented model records the last kick;
// expiry detection is exposed but the reference board wiring never resets
// the CPU mid-test (determinism beats authenticity here).
class Watchdog {
public:
    void kick(uint64_t cycle) { last_kick_ = cycle; kicked_ = true; }
    bool kicked() const { return kicked_; }
    uint64_t last_kick() const { return last_kick_; }
    bool expired(uint64_t cycle, uint64_t timeout) const {
        return kicked_ && (cycle - last_kick_ > timeout);
    }

private:
    uint64_t last_kick_ = 0;
    bool kicked_ = false;
};

// One line of the scripted input protocol: the three input-latch bytes.
struct InputFrame {
    uint8_t port0 = 0, port1 = 0, port2 = 0;
};

// Input port semantics (chapter-defined, documented in SPEC.md):
//   port 0 : coins / service          (latch byte 0)
//   port 1 : fire / move / 1P-2P start (latch byte 1)
//            bit0 left, bit1 right, bit2 fire, bit3 1P start, bit4 2P start
//   port 2 : dip switches             (latch byte 2)
//   port 3 : SHIFT REGISTER RESULT (read-only view of the shifter)
//   other  : floating low (0x00)
class IoPorts {
public:
    void attach(ShiftRegister* sr, SoundRecorder* snd, Watchdog* wdt) {
        shifter_ = sr;
        sound_ = snd;
        watchdog_ = wdt;
    }

    void set_inputs(const InputFrame& f) { inputs_ = f; }
    const InputFrame& inputs() const { return inputs_; }

    uint8_t in(uint8_t port) const {
        switch (port) {
            case 0: return inputs_.port0;
            case 1: return inputs_.port1;
            case 2: return inputs_.port2;
            case 3: return shifter_ ? shifter_->read() : uint8_t(0x00);
            default: return 0x00;
        }
    }

    void out(uint8_t port, uint8_t val, uint64_t cycle) {
        switch (port) {
            case 2:
                if (shifter_) shifter_->set_amount(val);
                break;
            case 3:
                if (sound_) sound_->record(cycle, port, val);
                break;
            case 4:
                if (shifter_) shifter_->write_data(val);
                break;
            case 5:
                if (sound_) sound_->record(cycle, port, val);
                break;
            case 6:
                if (watchdog_) watchdog_->kick(cycle);
                if (sound_) sound_->record(cycle, port, val);
                break;
            default:
                break;   // unassigned ports: write vanishes
        }
    }

private:
    InputFrame inputs_{};
    ShiftRegister* shifter_ = nullptr;
    SoundRecorder* sound_ = nullptr;
    Watchdog* watchdog_ = nullptr;
};

// ---------------------------------------------------------------------------
// Dual one-shot vblank timers
// ---------------------------------------------------------------------------

struct IrqRaise {
    bool raised = false;
    uint8_t opcode = 0;
};

// One shot fires per frame period; even frames jam RST 08, odd frames jam
// RST 10. `poll` is called before every CPU step with the cumulative
// cycle count; it reports at most one raise per call because a single
// instruction can never span a whole frame.
class VblankTimers {
public:
    VblankTimers() { configure(kCyclesPerFrame, kIrqOpcodeEven, kIrqOpcodeOdd); }

    void configure(uint64_t cycles_per_frame, uint8_t opcode_even,
                   uint8_t opcode_odd) {
        cpf_ = cycles_per_frame;
        op_even_ = opcode_even;
        op_odd_ = opcode_odd;
        reset();
    }

    void reset() {
        next_fire_ = cpf_;
        even_frame_ = true;
    }

    IrqRaise poll(uint64_t cycles_now) {
        IrqRaise r;
        if (cycles_now >= next_fire_) {
            r.raised = true;
            r.opcode = even_frame_ ? op_even_ : op_odd_;
            even_frame_ = !even_frame_;
            next_fire_ += cpf_;
        }
        return r;
    }

    uint64_t next_fire() const { return next_fire_; }
    bool even_frame() const { return even_frame_; }

private:
    uint64_t cpf_ = kCyclesPerFrame;
    uint64_t next_fire_ = cpf_;
    bool even_frame_ = true;
    uint8_t op_even_ = kIrqOpcodeEven;
    uint8_t op_odd_ = kIrqOpcodeOdd;
};

}  // namespace si
