// boot_bus.hpp — the 256-byte DMG boot ROM as an overlay Device.
//
// Hardware nuance: on reset, a 256-byte internal boot ROM sits at
// 0000-00FF IN FRONT of the cartridge (table order decides: the boot
// entry precedes the cart entry, and first match wins). ANY write to
// FF50 unmaps it — value ignored — revealing the cartridge beneath.
//
// Divergence from real silicon, documented on purpose: a real DMG ALSO
// unmaps automatically when the program counter crosses $0050 during
// execution (the "$0050 trigger"), because the boot ROM's own jump-out
// path is what hands control to the game. Our model keys on the FF50
// write ONLY — there is no CPU here to observe — so code that somehow
// skipped past $0050 without writing FF50 would still see the overlay.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "../01_range_table/bus.hpp"
#include "../02_device_attach/machine.hpp"
#include "../03_echo_alias/echo_bus.hpp"

namespace gbboot {

constexpr uint16_t kBootSize = 0x0100;   // 0000-00FF
constexpr uint16_t kBootDisable = 0xFF50;

using BootImage = std::array<uint8_t, kBootSize>;

// Deterministic stand-in for the real 256-byte boot program.
inline BootImage makeSyntheticBoot() {
    BootImage img{};
    for (size_t i = 0; i < img.size(); ++i)
        img[i] = static_cast<uint8_t>(i * 13 + 0x40);
    return img;
}

// The masked boot image. Reads come straight out of the array; writes
// are dropped (it is ROM).
class BootRom : public gbmap::Device {
public:
    explicit BootRom(const BootImage& image) : img_(image.data()) {}

    //@LABS-BEGIN 1
    //@LABS-SOLUTION
    // The bus guarantees addr <= 0x00FF while this device is attached:
    // its table entry spans exactly one page.
    uint8_t read(uint16_t addr) override { return img_[addr]; }
    void write(uint16_t, uint8_t) override {}  // masked ROM
    //@LABS-STUB
    uint8_t read(uint16_t) override {
        return 0x00;  // TODO(1): serve bytes from the boot image
    }
    void write(uint16_t, uint8_t) override {}
    //@LABS-END

private:
    const uint8_t* img_;
};

// Sits at exactly FF50-FF50, in FRONT of the I/O window. Its only job:
// notice the unmap handshake and pull the boot entry off the bus.
class Ff50Unmapper : public gbmap::Device {
public:
    Ff50Unmapper(gbmap::Bus* bus, gbmap::Device* boot)
        : bus_(bus), boot_(boot) {}

    bool bootMapped() const { return bootMapped_; }

    // Reads of FF50 carry no data on real hardware either; our open-bus
    // policy answers $00.
    uint8_t read(uint16_t) override { return 0x00; }

    //@LABS-BEGIN 2
    //@LABS-SOLUTION
    void write(uint16_t addr, uint8_t) override {
        if (addr == kBootDisable) {  // ANY value counts — the address is the message
            bus_->detach(boot_);
            bootMapped_ = false;
        }
    }
    //@LABS-STUB
    void write(uint16_t addr, uint8_t val) override {
        (void)addr;  // TODO(2): any write to $FF50 removes the boot overlay
        (void)val;
    }
    //@LABS-END

private:
    gbmap::Bus* bus_;
    gbmap::Device* boot_;
    bool bootMapped_ = true;
};

//@LABS-BEGIN 3
//@LABS-SOLUTION
// Overlay priority comes from TABLE ORDER: attachFront puts these entries
// ahead of everything already registered, so 0000-00FF resolves to the
// boot image and FF50 resolves to the trap before the I/O window sees it.
inline void attachBootOverlay(gbmap::Bus& bus, BootRom& boot,
                              Ff50Unmapper& unmapper) {
    bus.attachFront(kBootDisable, kBootDisable, &unmapper);
    bus.attachFront(0x0000, static_cast<uint16_t>(kBootSize - 1), &boot);
}
//@LABS-STUB
inline void attachBootOverlay(gbmap::Bus& bus, BootRom& boot,
                              Ff50Unmapper& unmapper) {
    // TODO(3): these must go IN FRONT of existing entries — appended
    // entries never win the first-match scan against the cart.
    bus.attach(0x0000, static_cast<uint16_t>(kBootSize - 1), &boot);
    bus.attach(kBootDisable, kBootDisable, &unmapper);
}
//@LABS-END

// Full machine with the boot overlay armed, exactly as reset leaves it.
// Members are declared in pointer-dependency order so every {&x} brace
// initialization observes an already-built object.
struct BootMachine {
    std::vector<uint8_t> cartImg;
    BootImage bootImg = makeSyntheticBoot();
    gbmap::Ram wram{0xC000, 0xDFFF};
    gbmap::Ram ioRegs{0xFF00, 0xFF7F};
    gbmap::Ram hram{0xFF80, 0xFFFE};
    gbmachine::IeLatch ie;
    gbmap::Bus bus;
    gbecho::EchoWindow echo{&wram};
    BootRom boot{bootImg};
    Ff50Unmapper unmapper{&bus, &boot};
    gbmachine::CartRom cart;

    explicit BootMachine(const std::vector<uint8_t>& cartImage)
        : cartImg(cartImage), cart(cartImg.data(), cartImg.size()) {
        wire();
    }

    void wire() {
        // Base map (same ranges as exercise 02, minus OAM for brevity):
        bus.attach(0x0000, 0x7FFF, &cart);
        bus.attach(0xC000, 0xDFFF, &wram);
        gbecho::attachEchoWindow(bus, echo);
        bus.attach(0xFF00, 0xFF7F, &ioRegs);
        bus.attach(0xFF80, 0xFFFE, &hram);
        bus.attach(0xFFFF, 0xFFFF, &ie);
        // Then the overlay, front-most:
        attachBootOverlay(bus, boot, unmapper);
    }
};

}  // namespace gbboot
