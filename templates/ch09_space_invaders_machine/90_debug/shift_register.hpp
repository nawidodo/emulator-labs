#pragma once
#include <cstdint>

// Debugging exercise — the shift register with a SEEDED BUG.
//
// The skeleton's implementation compiles, runs, and passes casual
// eyeballing. It is wrong in exactly one detail. Find it by testing
// against the contract (see DEBUGGING.md), not by staring.

class ShiftRegister {
public:
    void write_data(uint8_t v) { sr_ = uint16_t((sr_ >> 8) | (uint16_t(v) << 8)); }

    void set_amount(uint8_t amt) { amount_ = amt & 0x07; }
    uint8_t amount() const { return amount_; }

    uint8_t read() const {
//@LABS-BEGIN 1
//@LABS-STUB
        // BUG(1): this reads the window one position off from the real
        // hardware for every setting — and wraps badly at the top.
        // TODO(1): restore the true hardware window.
        return uint8_t(sr_ >> ((amount_ + 1) & 7));
//@LABS-SOLUTION
        return uint8_t(sr_ >> amount_);
//@LABS-END
    }

    uint16_t raw() const { return sr_; }

private:
    uint16_t sr_ = 0;
    uint8_t amount_ = 0;
};
