// machine.hpp — the full Game Boy machine map built by attaching one
// device object per decoded range to the exercise-01 Bus.
//
// Nothing here needs a CPU: this is pure bus mechanics. The map is the
// authoritative one from LECTURE.md; FEA0-FEFF stays UNATTACHED on
// purpose so the documented unusable-page policy handles it.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "../01_range_table/bus.hpp"

namespace gbmachine {

// Cartridge ROM: read-only window over an image. Writes are dropped —
// no MBC banking exists in this chapter, the image is the cart.
class CartRom : public gbmap::Device {
public:
    CartRom(const uint8_t* image, size_t size) : img_(image), size_(size) {}

    //@LABS-BEGIN 1
    //@LABS-SOLUTION
    // Addresses past the end of a short image pad as $FF — carts decode
    // their full 32 KiB window even when the file is smaller.
    uint8_t read(uint16_t addr) override {
        return static_cast<size_t>(addr) < size_ ? img_[addr] : 0xFF;
    }
    void write(uint16_t, uint8_t) override {}  // masked ROM: writes vanish
    //@LABS-STUB
    uint8_t read(uint16_t) override {
        return 0xFF;  // TODO(1): serve bytes from the committed cart image
    }
    void write(uint16_t, uint8_t) override {}
    //@LABS-END

private:
    const uint8_t* img_;
    size_t size_;
};

// The interrupt-enable register at FFFF is a single byte of state that
// happens to live at the very top of the map.
class IeLatch : public gbmap::Device {
public:
    uint8_t read(uint16_t) override { return value_; }
    void write(uint16_t, uint8_t val) override { value_ = val; }

private:
    uint8_t value_ = 0x00;
};

struct Machine {
    // VRAM 8000-9FFF, external RAM A000-BFFF, WRAM C000-DFFF,
    // OAM FE00-FE9F, I/O FF00-FF7F, HRAM FF80-FFFE, IE at FFFF.
    gbmap::Ram vram{0x8000, 0x9FFF};
    gbmap::Ram extRam{0xA000, 0xBFFF};
    gbmap::Ram wram{0xC000, 0xDFFF};
    gbmap::Ram oam{0xFE00, 0xFE9F};
    gbmap::Ram ioRegs{0xFF00, 0xFF7F};
    gbmap::Ram hram{0xFF80, 0xFFFE};
    IeLatch ie;
    gbmap::Bus bus;

    explicit Machine(const std::vector<uint8_t>& cartImage)
        : cart(cartImage.data(), cartImage.size()) {
        reset();
    }

    CartRom& rom() { return cart; }

    void reset() {
        attachLowDevices();
        attachHighDevices();
    }

    CartRom cart;
   private:
    //@LABS-BEGIN 2
    //@LABS-SOLUTION
    // Attach order is irrelevant for correctness — the Bus keeps its table
    // sorted by base address — but each range must land exactly once.
    void attachLowDevices() {
        bus.attach(0x0000, 0x7FFF, &cart);
        bus.attach(0x8000, 0x9FFF, &vram);
        bus.attach(0xA000, 0xBFFF, &extRam);
    }
    //@LABS-STUB
    void attachLowDevices() {
        (void)this;  // TODO(2): attach cartridge ROM, VRAM and external RAM
    }
    //@LABS-END

    //@LABS-BEGIN 3
    //@LABS-SOLUTION
    void attachHighDevices() {
        bus.attach(0xC000, 0xDFFF, &wram);
        bus.attach(0xFE00, 0xFE9F, &oam);
        bus.attach(0xFF00, 0xFF7F, &ioRegs);
        bus.attach(0xFF80, 0xFFFE, &hram);
        bus.attach(0xFFFF, 0xFFFF, &ie);
    }
    //@LABS-STUB
    void attachHighDevices() {
        (void)this;  // TODO(3): attach WRAM, OAM, I/O, HRAM and the IE latch
    }
    //@LABS-END
};



// Deterministic test cart: byte i carries a pattern derived from i, so
// any misrouting shows up as a wrong-but-plausible value.
inline std::vector<uint8_t> makeTestCart() {
    std::vector<uint8_t> img(0x8000);
    for (size_t i = 0; i < img.size(); ++i)
        img[i] = static_cast<uint8_t>(i * 7 + 0x11);
    return img;
}

}  // namespace gbmachine
