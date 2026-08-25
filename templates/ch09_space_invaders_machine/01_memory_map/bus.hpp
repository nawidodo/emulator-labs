#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

// Exercise 1 — the Space Invaders memory map as bus devices.
//
//   0000-1FFF  ROM  : 8 KiB = four independent 2 KiB banks, linear
//   2000-23FF  RAM  : 1 KiB work RAM
//   2400-3FFF  VRAM : 7 KiB 1bpp framebuffer
//
// The board has NO mirroring: each window covers its range exactly once
// and nothing aliases anywhere else. Unmapped reads float low (0x00) and
// unmapped writes drop — documented stand-ins for floating-bus behavior.
//
// Devices see LOCAL offsets (bus address minus window base); the decoder
// does that subtraction exactly once.

namespace si {

constexpr uint16_t kRomBase  = 0x0000;
constexpr size_t   kRomSize  = 0x2000;   // 8 KiB = four 2 KiB banks
constexpr size_t   kRomBank  = 0x0800;
constexpr uint16_t kRamBase  = 0x2000;
constexpr size_t   kRamSize  = 0x0400;   // 1 KiB
constexpr uint16_t kVramBase = 0x2400;
constexpr size_t   kVramSize = 0x1C00;   // 7 KiB

class BusDevice {
public:
    virtual ~BusDevice() = default;
    virtual uint8_t read(uint16_t addr) const = 0;
    virtual void write(uint16_t addr, uint8_t val) = 0;
};

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

    uint8_t read(uint16_t addr) const {
//@LABS-BEGIN 1
//@LABS-SOLUTION
        // Banks sit linearly: any address in 0000-1FFF indexes the image
        // directly (the mask IS the bank select — there is none).
        return rom_[addr & (kRomSize - 1)];
//@LABS-STUB
        // TODO(1): return the ROM byte for a bus address 0x0000-0x1FFF.
        // The four 2 KiB banks map linearly; writes never reach here.
        (void)addr;
        return 0x00;  // wrong on purpose: ROM reads float low
//@LABS-END
    }

    void write(uint16_t addr, uint8_t val) {
        // Masked ROM absorbs writes unconditionally — no exception, no
        // bank latch. Nothing to implement; kept for interface parity.
        (void)addr;
        (void)val;
    }

private:
    uint8_t rom_[kRomSize] = {};
};

class RamDevice final : public BusDevice {
public:
    static constexpr size_t kSize = kRamSize;

    uint8_t read(uint16_t addr) const {
//@LABS-BEGIN 2
//@LABS-SOLUTION
        // Local offset is guaranteed in-range when routed through the
        // decoder; the guard keeps direct construction safe.
        return addr < kSize ? ram_[addr] : uint8_t(0x00);
//@LABS-STUB
        // TODO(2): return the RAM byte at local offset `addr` (0..0x3FF).
        (void)addr;
        return 0x00;  // wrong on purpose: RAM reads float low
//@LABS-END
    }

    void write(uint16_t addr, uint8_t val) {
//@LABS-BEGIN 3
//@LABS-SOLUTION
        if (addr < kSize) ram_[addr] = val;
//@LABS-STUB
        // TODO(3): store `val` at local offset `addr` (ignore out-of-range).
        (void)addr;
        (void)val;
//@LABS-END
    }

    uint8_t* bytes() { return ram_; }
    const uint8_t* bytes() const { return ram_; }

private:
    uint8_t ram_[kSize] = {};
};

class VramDevice final : public BusDevice {
public:
    static constexpr size_t kSize = kVramSize;

    uint8_t read(uint16_t addr) const {
//@LABS-BEGIN 4
//@LABS-SOLUTION
        return addr < kSize ? vram_[addr] : uint8_t(0x00);
//@LABS-STUB
        // TODO(4): return the VRAM byte at local offset `addr`
        // (0..0x1BFF).
        (void)addr;
        return 0x00;  // wrong on purpose: VRAM reads float low
//@LABS-END
    }

    void write(uint16_t addr, uint8_t val) {
//@LABS-BEGIN 5
//@LABS-SOLUTION
        if (addr < kSize) vram_[addr] = val;
//@LABS-STUB
        // TODO(5): store `val` at local offset `addr` (0..0x1BFF).
        (void)addr;
        (void)val;
//@LABS-END
    }

    const uint8_t* bytes() const { return vram_; }

private:
    uint8_t vram_[kSize] = {};
};

// Range router: devices attach to disjoint address windows; the FIRST
// matching window wins. This is the machine's memory-side bus object;
// the machine layer (exercise 5) plugs it under the CPU's Bus interface.
class AddressDecoder {
public:
    struct Window {
        uint16_t lo, hi;
        BusDevice* dev;
    };

    void attach(uint16_t lo, uint16_t hi, BusDevice* dev) {
        map_.push_back({lo, hi, dev});
    }
    const std::vector<Window>& map() const { return map_; }

    uint8_t read(uint16_t addr) const {
//@LABS-BEGIN 6
//@LABS-SOLUTION
        // First matching window wins; the device sees address - lo.
        for (const Window& w : map_)
            if (addr >= w.lo && addr <= w.hi)
                return w.dev->read(uint16_t(addr - w.lo));
        return 0x00;   // unmapped: floating bus reads low
//@LABS-STUB
        // TODO(6): route `addr` through the attached windows (first match
        // wins, subtract the window base before calling the device) and
        // return 0x00 when no window matches.
        (void)addr;
        return 0x00;
//@LABS-END
    }

    void write(uint16_t addr, uint8_t val) {
//@LABS-BEGIN 7
//@LABS-SOLUTION
        for (const Window& w : map_)
            if (addr >= w.lo && addr <= w.hi) {
                w.dev->write(uint16_t(addr - w.lo), val);
                return;
            }
        // Unmapped writes drop on the floor.
//@LABS-STUB
        // TODO(7): forward writes like reads; silently drop unmapped ones.
        (void)addr;
        (void)val;
//@LABS-END
    }

private:
    std::vector<Window> map_;
};

}  // namespace si
