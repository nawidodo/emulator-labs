#pragma once
//
// ch40 / 90_debug — PS1 interrupt controller WITH A SEEDED BUG
//
// This exercise hands you an interrupt controller whose acknowledge path
// is broken. The register model otherwise matches 01_irq_controller; only
// the @LABS-marked ack() differs between the buggy starter and the fixed
// reference. See DEBUGGING.md for the observed symptoms.

#include <cstdint>

namespace ps1::sysdev {

constexpr uint32_t kIrqVblank     = 1u << 0;   // 0x001
constexpr uint32_t kIrqGpu        = 1u << 1;   // 0x002 (GP0(1Fh), rare)
constexpr uint32_t kIrqCdrom      = 1u << 2;   // 0x004
constexpr uint32_t kIrqDma        = 1u << 3;   // 0x008
constexpr uint32_t kIrqTimer0     = 1u << 4;   // 0x010
constexpr uint32_t kIrqTimer1     = 1u << 5;   // 0x020
constexpr uint32_t kIrqTimer2     = 1u << 6;   // 0x040
constexpr uint32_t kIrqController = 1u << 7;   // 0x080 (SIO0 pad/memcard)
constexpr uint32_t kIrqSpu        = 1u << 8;   // 0x100
constexpr uint32_t kIrqPio        = 1u << 9;   // 0x200 (expansion port)
constexpr uint32_t kIrqSio        = 1u << 10;  // 0x400 (SIO(2))

constexpr uint32_t kAllIrqSources = kIrqVblank | kIrqGpu | kIrqCdrom |
    kIrqDma | kIrqTimer0 | kIrqTimer1 | kIrqTimer2 | kIrqController |
    kIrqSpu | kIrqPio | kIrqSio;

class IrqController {
public:
    void write_mask(uint32_t v) { mask_ = v & kAllIrqSources; }
    uint32_t read_mask() const { return mask_; }

    void raise(uint32_t lines) {
        levels_ |= lines & kAllIrqSources;
        status_ |= lines & kAllIrqSources;
    }

    //@LABS-BEGIN 1
    //@LABS-SOLUTION
    void ack(uint32_t value) {
        status_ &= ~(value & kAllIrqSources);
        // Still-asserted lines re-latch immediately: acknowledging the
        // latch does not service the peripheral behind it.
        status_ |= levels_;
    }
    //@LABS-STUB
    void ack(uint32_t value) {
        (void)value;
        // TODO(1): this looks like a perfectly working acknowledge...
        status_ = 0;
    }
    //@LABS-END

    void lower(uint32_t lines) { levels_ &= ~(lines & kAllIrqSources); }

    uint32_t status() const { return status_; }

    bool irq_out() const { return (status_ & mask_) != 0; }

private:
    uint32_t status_ = 0;
    uint32_t mask_ = 0;
    uint32_t levels_ = 0;
};

}  // namespace ps1::sysdev
