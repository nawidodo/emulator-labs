#pragma once
#include <cstdint>

// Debugging variant of 01_digital_pad/pad.hpp (namespace siodbg) carrying
// SEEDED BUG: the ACK polarity is inverted. The @LABS SOLUTION side below is
// the correct reference; the STUB side carries the bug.
#include "../01_digital_pad/pad.hpp"

namespace siodbg {

using sio::Buttons;
using sio::BTN_CIRCLE;
using sio::BTN_CROSS;
using sio::BTN_DOWN;
using sio::BTN_L1;
using sio::BTN_L2;
using sio::BTN_L3;
using sio::BTN_LEFT;
using sio::BTN_R1;
using sio::BTN_R2;
using sio::BTN_R3;
using sio::BTN_RIGHT;
using sio::BTN_SELECT;
using sio::BTN_SQUARE;
using sio::BTN_START;
using sio::BTN_TRIANGLE;
using sio::BTN_UP;
using sio::kPadCmdRead;
using sio::kPadIdHi;
using sio::kPadIdLo;
using sio::kSelectPad;

class DigitalPad {
public:
    bool selected() const { return selected_; }
    void set_buttons(const Buttons& b) { buttons_ = b; }

    void select(bool low) {
        if (selected_ && !low) {
            armed_ = false;
            count_ = 0;
        }
        selected_ = low;
    }

    // True while a read transaction is in flight (data still owed).
    bool busy() const { return selected_ && armed_ && count_ <= 3; }

    uint8_t handle(uint8_t tx) {
        if (!selected_) return 0xFF;
        if (!armed_) {
            armed_ = (tx == kPadCmdRead);
            count_ = 0;
            return 0xFF;
        }
        uint8_t resp = 0xFF;
        switch (count_) {
            case 0: resp = kPadIdLo; break;
            case 1: resp = kPadIdHi; break;
            case 2:
                resp =
                    static_cast<uint8_t>(sio::report_word(buttons_) & 0xFF);
                break;
            case 3:
                resp = static_cast<uint8_t>(sio::report_word(buttons_) >> 8);
                break;
            default: return 0xFF;
        }
        ++count_;
        return resp;
    }

//@LABS-BEGIN 1
//@LABS-SOLUTION
    // Correct ACK: asserted after every response byte that still has a
    // successor, i.e. while count_ is 1..3 inside an armed session.
    bool ack() const {
        return selected_ && armed_ && count_ >= 1 && count_ <= 3;
    }
//@LABS-STUB
    // TODO(1): BUG SEEDED — this returns the INVERTED polarity. Fix it so
    // ACK is asserted exactly while the pad still owes data (count_ in
    // 1..3 of an armed, selected session).
    bool ack() const {
        return !(selected_ && armed_ && count_ >= 1 && count_ <= 3);
    }
//@LABS-END

private:
    bool selected_ = false;
    bool armed_ = false;
    int count_ = 0;
    Buttons buttons_{};
};

}  // namespace siodbg
