#pragma once
#include <cstdint>

// Exercise 2 — the 8-bit hardware shift register behind ports 2/3/4.
//
// This peripheral existed because drawing 8-pixel-tall sprites byte-wise
// was cheaper than a bit-plane blitter. The TTL part is simple and its
// behavior is EXACTLY:
//
//   OUT 2 : bits 0-2 latch the shift amount. The counter is physically
//           3 bits wide, so amounts wrap modulo 8 (writing 7 then
//           "incrementing" gives 0).
//   OUT 4 : shifts the whole 16-bit register right one BYTE (old high
//           half becomes the new low half) and drops the written byte
//           into the high half. Two successive writes therefore fill LOW
//           byte first, HIGH byte second ("write-low/high" filling).
//   IN 3  : returns bits [amount .. amount+7] of the 16-bit register —
//           an 8-bit window slid along by the amount counter.
//
class ShiftRegister {
public:
    // OUT 4: old high half -> low half, written byte -> high half.
    void write_data(uint8_t v) {
        sr_ = uint16_t((sr_ >> 8) | (uint16_t(v) << 8));
    }

    // OUT 2: only bits 0-2 reach the counter; everything else is ignored.
    void set_amount(uint8_t amt) {
//@LABS-BEGIN 1
//@LABS-SOLUTION
        amount_ = amt & 0x07;   // 3-bit hardware counter wraps modulo 8
//@LABS-STUB
        // TODO(1): latch the shift amount from the low THREE bits of
        // `amt` (the counter wraps modulo 8: set_amount(9) == set_amount(1)).
        (void)amt;
        amount_ = 0;
//@LABS-END
    }
    uint8_t amount() const { return amount_; }

    // IN 3: the 8-bit window at bit offset `amount`.
    uint8_t read() const {
//@LABS-BEGIN 2
//@LABS-SOLUTION
        return uint8_t(sr_ >> amount_);
//@LABS-STUB
        // TODO(2): return bits [amount .. amount+7] of the 16-bit
        // register as one byte.
        (void)amount_;
        return 0x00;  // wrong on purpose: reads float low
//@LABS-END
    }

    // Test/inspection hook: the raw 16-bit register contents.
    uint16_t raw() const { return sr_; }

private:
    uint16_t sr_ = 0;
    uint8_t amount_ = 0;
};
