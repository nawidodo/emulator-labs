// echo_bus.hpp — the E000-FDFF echo window as a first-class Device.
//
// Hardware fact: the address decoder saves a chip-select by mapping the
// LOWER 0x1E00 bytes of the 8 KiB WRAM bank twice. Echo reads and
// writes hit the SAME cells as C000-DDFF; the exact translation is
// (addr - $2000), so FDFF-$2000 = DDFF is the last aliased byte.
// The tail DFFF has no echo counterpart, and nothing above FDFF is
// aliased either (FE00 is OAM territory).
#pragma once

#include <cstdint>

#include "../01_range_table/bus.hpp"

namespace gbecho {

constexpr uint16_t kWramBase = 0xC000;
constexpr uint16_t kEchoBase = 0xE000;
constexpr uint16_t kEchoEnd = 0xFDFF;  // inclusive
constexpr uint16_t kEchoDelta = 0x2000;

// A one-directional alias: whatever device it wraps sees translated
// addresses, so no RAM cell knows echo traffic ever happened.
class EchoWindow : public gbmap::Device {
public:
    explicit EchoWindow(gbmap::Device* target) : target_(target) {}

    //@LABS-BEGIN 1
    //@LABS-SOLUTION
    uint8_t read(uint16_t addr) override {
        return target_->read(static_cast<uint16_t>(addr - kEchoDelta));
    }
    //@LABS-STUB
    uint8_t read(uint16_t addr) override {
        (void)addr;  // TODO(1): translate by -$2000 before touching the target
        return target_->read(kWramBase);
    }
    //@LABS-END

    //@LABS-BEGIN 2
    //@LABS-SOLUTION
    void write(uint16_t addr, uint8_t val) override {
        target_->write(static_cast<uint16_t>(addr - kEchoDelta), val);
    }
    //@LABS-STUB
    void write(uint16_t addr, uint8_t val) override {
        (void)addr;  // TODO(2): translate by -$2000 before touching the target
        target_->write(kWramBase, val);
    }
    //@LABS-END

private:
    gbmap::Device* target_;
};

//@LABS-BEGIN 3
//@LABS-SOLUTION
// Registers the echo range on the bus. WRAM itself must already be
// attached at C000-DFFF; both entries point at overlapping addresses
// only through the translation, never directly.
inline void attachEchoWindow(gbmap::Bus& bus, EchoWindow& echo) {
    bus.attach(kEchoBase, kEchoEnd, &echo);
}
//@LABS-STUB
inline void attachEchoWindow(gbmap::Bus& bus, EchoWindow& echo) {
    (void)bus;      // TODO(3): register E000-FDFF on the bus
    (void)echo;
}
//@LABS-END

// Minimal machine for echo drills: WRAM plus its mirror, nothing else.
struct WramWithEcho {
    gbmap::Ram wram{kWramBase, 0xDFFF};
    EchoWindow echo{&wram};
    gbmap::Bus bus;

    WramWithEcho() {
        bus.attach(kWramBase, 0xDFFF, &wram);
        attachEchoWindow(bus, echo);
    }
};

}  // namespace gbecho
