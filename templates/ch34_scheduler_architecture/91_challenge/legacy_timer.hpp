#pragma once
// Legacy ch13-style tick-driven down-counter. Reference implementation —
// do not modify; the event-driven adapter must match it bit-exactly.
#include <cstdint>

namespace legacy {

class LegacyTimer {
public:
    void set_period(uint16_t p) { stored_period_ = p; }

    void tick() {
        if (stored_period_ == 0) return;  // stopped
        if (counter_ > 0) {
            --counter_;
            if (counter_ == 0) {
                flag_ = true;
                counter_ = stored_period_;  // reload from STORED period
            }
        }
    }

    bool flag() const { return flag_; }
    void clear_flag() { flag_ = false; }
    uint16_t counter() const { return counter_; }

    // Test hook: start counting (power-on loads the stored period).
    void arm(uint16_t p) {
        stored_period_ = p;
        counter_ = p;
    }

private:
    uint16_t stored_period_ = 0;
    uint16_t counter_ = 0;
    bool flag_ = false;
};

}  // namespace legacy
