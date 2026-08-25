#pragma once
#include <cstdint>

namespace sio {

// ---------------------------------------------------------------------------
// Digital pad (controller) on the PS1 SIO bus.
//
// Framing: the host pulls /SELECT low for one device and clocks bytes through
// the TX/RX register at 1F801040. The FIRST transmitted byte of a session is
// the device select: 0x01 addresses a pad, 0x81 a memory card. Bits 12/13 of
// CTRL (1F80104A) decide WHICH SLOT answers.
//
// Digital pad read protocol (this course's model, pinned by tests):
//   tx: 01 | 42 | 00 | 00 | 00 | 00
//   rx: FF | FF | 41 | 5A | b.lo | b.hi
// i.e. after arming on command 0x42 the pad returns two ID bytes — ID word
// 0x5A41 with the LOW byte first — followed by the active-low button halfword,
// low byte first. Further clocks return 0xFF until /SELECT rises.
//
// The button halfword is ACTIVE-LOW like real hardware reports:
// bit set (1) = released, bit clear (0) = pressed. Bit layout (bit 0 = LSB):
//   0 select, 1 l3, 2 r3, 3 start, 4 up, 5 right, 6 down, 7 left,
//   8 l2, 9 r2, 10 l1, 11 r1, 12 triangle, 13 circle, 14 cross, 15 square
// A digital pad never presses L3/R3, so those bits always report released.
//
// ACK: after every response byte EXCEPT THE FINAL ONE the pad asserts ACK
// (drives the line low). We expose this combinationally as ack(); the bus
// maps it onto STAT bit 7 with a NON-inverted mapping (STAT.7 == 1 while ACK
// is asserted) — a deliberate simplification documented in LECTURE.md.
// ---------------------------------------------------------------------------

constexpr uint8_t kSelectPad = 0x01;
constexpr uint8_t kPadCmdRead = 0x42;
constexpr uint8_t kPadIdLo = 0x41;
constexpr uint8_t kPadIdHi = 0x5A;

enum ButtonBits : uint16_t {
    BTN_SELECT = 1u << 0,
    BTN_L3 = 1u << 1,
    BTN_R3 = 1u << 2,
    BTN_START = 1u << 3,
    BTN_UP = 1u << 4,
    BTN_RIGHT = 1u << 5,
    BTN_DOWN = 1u << 6,
    BTN_LEFT = 1u << 7,
    BTN_L2 = 1u << 8,
    BTN_R2 = 1u << 9,
    BTN_L1 = 1u << 10,
    BTN_R1 = 1u << 11,
    BTN_TRIANGLE = 1u << 12,
    BTN_CIRCLE = 1u << 13,
    BTN_CROSS = 1u << 14,
    BTN_SQUARE = 1u << 15,
};

struct Buttons {
    bool select = false, l3 = false, r3 = false, start = false;
    bool up = false, right = false, down = false, left = false;
    bool l2 = false, r2 = false, l1 = false, r1 = false;
    bool triangle = false, circle = false, cross = false, square = false;
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Pack pressed-button booleans into the ACTIVE-HIGH bitfield above
// (bit set = pressed).
inline uint16_t pack_buttons(const Buttons& b) {
    uint16_t w = 0;
    if (b.select) w |= BTN_SELECT;
    if (b.l3) w |= BTN_L3;
    if (b.r3) w |= BTN_R3;
    if (b.start) w |= BTN_START;
    if (b.up) w |= BTN_UP;
    if (b.right) w |= BTN_RIGHT;
    if (b.down) w |= BTN_DOWN;
    if (b.left) w |= BTN_LEFT;
    if (b.l2) w |= BTN_L2;
    if (b.r2) w |= BTN_R2;
    if (b.l1) w |= BTN_L1;
    if (b.r1) w |= BTN_R1;
    if (b.triangle) w |= BTN_TRIANGLE;
    if (b.circle) w |= BTN_CIRCLE;
    if (b.cross) w |= BTN_CROSS;
    if (b.square) w |= BTN_SQUARE;
    return w;
}
//@LABS-STUB
// TODO(1): pack each pressed boolean into its ButtonBits position and return
// the active-high word. Return 0 for now so the suite compiles RED.
inline uint16_t pack_buttons(const Buttons&) {
    return 0;
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Wire representation: ACTIVE-LOW inversion of pack_buttons(), with L3/R3
// forced released because a digital pad has no sticks to click.
inline uint16_t report_word(const Buttons& b) {
    return static_cast<uint16_t>(~pack_buttons(b) | BTN_L3 | BTN_R3);
}
//@LABS-STUB
// TODO(2): return ~pack_buttons(b) with BTN_L3|BTN_R3 forced set (released).
// Return 0xFFFF (nothing pressed) for now.
inline uint16_t report_word(const Buttons&) {
    return 0xFFFF;
}
//@LABS-END

// Inverse of report_word(): rebuild Buttons from an active-low wire halfword.
inline Buttons buttons_from_report(uint16_t report) {
    const uint16_t pressed =
        static_cast<uint16_t>(~report & ~(BTN_L3 | BTN_R3));
    Buttons b;
    b.select = pressed & BTN_SELECT;
    b.start = pressed & BTN_START;
    b.up = pressed & BTN_UP;
    b.right = pressed & BTN_RIGHT;
    b.down = pressed & BTN_DOWN;
    b.left = pressed & BTN_LEFT;
    b.l2 = pressed & BTN_L2;
    b.r2 = pressed & BTN_R2;
    b.l1 = pressed & BTN_L1;
    b.r1 = pressed & BTN_R1;
    b.triangle = pressed & BTN_TRIANGLE;
    b.circle = pressed & BTN_CIRCLE;
    b.cross = pressed & BTN_CROSS;
    b.square = pressed & BTN_SQUARE;
    return b;
}

class DigitalPad {
public:
    bool selected() const { return selected_; }
    void set_buttons(const Buttons& b) { buttons_ = b; }

//@LABS-BEGIN 3
//@LABS-SOLUTION
    // /SELECT line. The RISING edge (deselect) must wipe any partial
    // transaction: a real pad forgets where it was mid-read when the host
    // releases the select line.
    void select(bool low) {
        if (selected_ && !low) {
            armed_ = false;
            count_ = 0;
        }
        selected_ = low;
    }
//@LABS-STUB
    // TODO(3): on the rising edge (!low while previously selected) clear the
    // armed flag and sequence counter so the next session starts from scratch.
    void select(bool low) {
        selected_ = low;
    }
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
    // Clock one transmitted byte in, get the response byte out. Bytes outside
    // a session draw 0xFF; the command byte arms the read; then follow the
    // four data bytes (ID lo, ID hi, buttons lo, buttons hi).
    uint8_t handle(uint8_t tx) {
        last_final_ = true;
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
                resp = static_cast<uint8_t>(report_word(buttons_) & 0xFF);
                break;
            case 3:
                resp = static_cast<uint8_t>(report_word(buttons_) >> 8);
                break;
            default: return 0xFF;  // overrun: no more data, stay at 0xFF
        }
        ++count_;
        // ACK stays asserted while another data byte is still owed.
        last_final_ = (count_ > 3);
        return resp;
    }
//@LABS-STUB
    // TODO(4): implement the read state machine described in the header
    // comment. For now always answer 0xFF so the suite compiles RED.
    uint8_t handle(uint8_t) {
        return 0xFF;
    }
//@LABS-END

//@LABS-BEGIN 5
//@LABS-SOLUTION
    // True between response bytes while the transaction still owes data:
    // after the responses 41, 5A and buttons-lo — but NOT after the final
    // buttons-hi byte and never outside an armed session.
    bool ack() const {
        return selected_ && armed_ && count_ >= 1 && count_ <= 3;
    }
//@LABS-STUB
    // TODO(5): ACK is asserted exactly while count_ is 1..3 inside an armed,
    // selected session. Return false for now.
    bool ack() const {
        return false;
    }
//@LABS-END

    // True while a read transaction is in flight (data still owed).
    bool busy() const { return selected_ && armed_ && count_ <= 3; }

private:
    bool selected_ = false;
    bool armed_ = false;
    bool last_final_ = true;
    int count_ = 0;          // next data byte index (0..3)
    Buttons buttons_{};
};

}  // namespace sio
