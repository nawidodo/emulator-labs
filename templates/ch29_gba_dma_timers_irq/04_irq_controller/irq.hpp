#pragma once
// GBA interrupt controller: IE/IF/IME raise-ack-wake-service flow.
//
// Hardware sets IF bits regardless of IE; the CPU only sees an interrupt
// when IME is set and IE & IF is nonzero. Acknowledging writes ones that
// CLEAR those IF bits. HALT wakes on any enabled pending source.
#include <cstdint>

namespace gba {

using u16 = uint16_t;
using u32 = uint32_t;
using s32 = int32_t;
using u64 = uint64_t;

// Well-known GBA IRQ flag bits.
enum : u16 {
    kIrqVBlank = 1 << 0,
    kIrqHBlank = 1 << 1,
    kIrqVCount = 1 << 2,
    kIrqTimer0 = 1 << 3,
    kIrqTimer1 = 1 << 4,
    kIrqTimer2 = 1 << 5,
    kIrqTimer3 = 1 << 6,
    kIrqDma0 = 1 << 8,
    kIrqDma1 = 1 << 9,
    kIrqDma2 = 1 << 10,
    kIrqDma3 = 1 << 11,
    kIrqKeypad = 1 << 12,
    kIrqGamePak = 1 << 13,
};

struct IrqController {
    u16 ie = 0;    // 0x04000102 low half: enable mask
    u16 iff = 0;   // same address, high half read: pending flags
    bool ime = false;

//@LABS-BEGIN 1
//@LABS-SOLUTION
    // Raise sources: IF records them whether or not IE/IME allow delivery.
    void raise(u16 bits) { iff |= bits; }

    // Acknowledge: write-1-to-clear semantics.
    void acknowledge(u16 bits) { iff &= u16(~bits); }

    // Would the CPU take an interrupt right now?
    bool pending() const { return ime && (ie & iff) != 0; }
//@LABS-STUB
    // TODO(1): implement raise (OR into IF), acknowledge (write-1-to-clear)
    // and pending (IME set AND some enabled flag raised).
    void raise(u16 bits) { (void)bits; }
    void acknowledge(u16 bits) { (void)bits; }
    bool pending() const { return false; }  // wrong on purpose
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
    // HALT wake check: any enabled source (even without IME? hardware wakes
    // the CPU only with IME set) — we require IME like real delivery.
    bool should_wake_halt() const { return pending(); }

    // Highest-priority service candidate: lowest set bit of IE & IF.
    // Returns the bit index (BIOS dispatch order), or -1 when none.
    int next_service_bit() const {
        u16 active = u16(ie & iff);
        for (int b = 0; b < 14; ++b)
            if (active & (1 << b)) return b;
        return -1;
    }
//@LABS-STUB
    // TODO(2): HALT wakes exactly when an interrupt would be delivered;
    // the BIOS services the LOWEST set bit of IE & IF first. Return -1
    // when nothing needs servicing.
    bool should_wake_halt() const { return false; }  // wrong on purpose
    int next_service_bit() const { return -1; }      // wrong on purpose
//@LABS-END
};

}  // namespace gba
