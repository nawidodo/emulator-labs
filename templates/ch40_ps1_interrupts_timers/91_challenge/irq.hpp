#pragma once
//
// ch40 / irq.hpp — PS1 interrupt controller register model
// (psx-spx section "Interrupt Control", ports 1F801070h/I_STAT and
// 1F801074h/I_MASK).
//
// The controller latches peripheral request lines into a status register,
// software acknowledges by writing ones to I_STAT (write-1-clears), and the
// CPU IRQ output is simply the masked status:
//
//     irq_out = (I_STAT & I_MASK) != 0
//
// A source whose raw line is STILL asserted re-latches its bit on every
// acknowledge: acknowledging I_STAT does not deassert the peripheral itself.
// Periodic sources (vblank each frame, timers each period) re-assert by
// raising again, so an acknowledged latch self-heals on the next event.
//
// psx-spx bit assignment (real silicon):
//   0 VBLANK  1 GPU  2 CDROM  3 DMA  4 TMR0  5 TMR1  6 TMR2
//   7 SIO0 controller/memcard  8 SPU  9 PIO/expansion  10 SIO

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
    // 1F801074h — I_MASK stores the value directly (0=disabled, 1=enabled).
    void write_mask(uint32_t v) { mask_ = v & kAllIrqSources; }
    uint32_t read_mask() const { return mask_; }

    // Peripheral side: assert one or more interrupt lines. Raising a line
    // that is already latched keeps the latch set (level semantics).
    void raise(uint32_t lines) {
        levels_ |= lines & kAllIrqSources;
        status_ |= lines & kAllIrqSources;
    }

    // 1F801070h write: acknowledge. Each 1 bit clears its status bit; 0
    // bits leave their status untouched (write-1-clears, like DMA DICR).
    void ack(uint32_t value) {
        status_ &= ~(value & kAllIrqSources);
        // Still-asserted lines re-latch immediately: acknowledging the
        // latch does not service the peripheral behind it.
        status_ |= levels_;
    }

    // Peripheral side: deassert a line (device handshake, e.g. JOY_CTRL.ACK
    // or reading the CD response FIFO). Clears only the raw level; the
    // latched status bit survives until software acknowledges it.
    void lower(uint32_t lines) { levels_ &= ~(lines & kAllIrqSources); }

    uint32_t status() const { return status_; }

    bool irq_out() const { return (status_ & mask_) != 0; }

private:
    uint32_t status_ = 0;  // latched request bits (read I_STAT)
    uint32_t mask_ = 0;    // enable bits (I_MASK)
    uint32_t levels_ = 0;  // raw peripheral lines, independent of the latch
};

}  // namespace ps1::sysdev
