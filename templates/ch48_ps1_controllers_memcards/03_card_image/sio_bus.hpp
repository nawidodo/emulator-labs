#pragma once
#include <cstdint>

#include "../01_digital_pad/pad.hpp"
#include "../02_card_protocol/memcard.hpp"

namespace sio {

// ---------------------------------------------------------------------------
// Dual-slot SIO bus: the register window 1F801040..1F80104E plus two
// independent device slots. Each slot carries one pad AND one memory card;
// the first transmitted byte of every session picks WHICH KIND answers and
// CTRL bits 12/13 pick WHICH SLOT.
//
// Registers modeled (all others read 0):
//   1040 TX/RX : xfer() pushes one byte, returns the response byte
//   1044 STAT  : bit0 TX ready (start), bit1 TX ready (finish) — always 1;
//                bit7 ACK level, NON-inverted: 1 while the armed device
//                holds ACK asserted (documented course simplification)
//   1048 MODE  : stored verbatim, no timing effect in the headless model
//   104A CTRL  : bit0 TXEN (0 = bus mute), bit4 ACK-clear (no side effect,
//                ACK is combinational here), bit5 reset devices,
//                bits 12/13 slot select — bit12 = slot 1, bit13 = slot 2;
//                if BOTH are set slot 2 wins; if NEITHER, nothing answers
//                and xfer() returns 0xFF
//   104E BAUD  : stored; each transferred byte advances the serial bit
//                counter by 8 (the only "timing" we model)
//
// Device-select bytes: 0x01 -> pad of the active slot, 0x81 -> its card.
// Selecting a different device mid-session implicitly deselects the old one.
// ---------------------------------------------------------------------------

constexpr uint16_t kSioData = 0x1F801040;
constexpr uint16_t kSioStat = 0x1F801044;

constexpr uint16_t CTRL_TXEN = 1u << 0;
constexpr uint16_t CTRL_ACK_IRQ_EN = 1u << 2;
constexpr uint16_t CTRL_RESET = 1u << 5;
constexpr uint16_t CTRL_SLOT1 = 1u << 12;
constexpr uint16_t CTRL_SLOT2 = 1u << 13;

constexpr uint16_t STAT_TXRDY = 1u << 0 | 1u << 1;
constexpr uint16_t STAT_ACK_LEVEL = 1u << 7;

class SioBus {
public:
    SioBus() { reset(); }

    DigitalPad pads[2];
    MemCard cards[2];

    void reset() {
        for (int i = 0; i < 2; ++i) {
            pads[i].select(false);
            cards[i].reset();
            cards[i].select(false);
        }
        ctrl_ = 0;
        mode_ = 0;
        baud_ = 0;
        armed_slot_ = -1;
        armed_kind_ = Kind::None;
        bits_ = 0;
    }

    void write_ctrl(uint16_t v) {
        ctrl_ = v;
        if (v & CTRL_RESET) {
            for (int i = 0; i < 2; ++i) {
                pads[i].select(false);
                cards[i].reset();
                cards[i].select(false);
            }
            armed_slot_ = -1;
            armed_kind_ = Kind::None;
        }
    }
    uint16_t ctrl() const { return ctrl_; }

    void write_mode(uint16_t v) { mode_ = v; }
    uint16_t mode() const { return mode_; }
    void write_baud(uint16_t v) { baud_ = v; }
    uint16_t baud() const { return baud_; }

    // Active slot from CTRL bits 12/13; -1 when neither is set. Slot 2 wins
    // if both bits are set.
    int active_slot() const {
        if (ctrl_ & CTRL_SLOT2) return 1;
        if (ctrl_ & CTRL_SLOT1) return 0;
        return -1;
    }

    uint64_t serial_bits() const { return bits_; }

    uint16_t read_stat() const {
        uint16_t s = STAT_TXRDY;
        if (ack_level()) s |= STAT_ACK_LEVEL;
        return s;
    }

    bool ack_level() const {
        if (armed_kind_ == Kind::Pad && armed_slot_ >= 0)
            return pads[armed_slot_].ack();
        if (armed_kind_ == Kind::Card && armed_slot_ >= 0)
            return cards[armed_slot_].ack();
        return false;
    }

    // True while the armed device is mid-transaction.
    bool session_busy() const {
        if (armed_kind_ == Kind::Pad && armed_slot_ >= 0)
            return pads[armed_slot_].busy();
        if (armed_kind_ == Kind::Card && armed_slot_ >= 0)
            return cards[armed_slot_].busy();
        return false;
    }

    // One byte through 1F801040. Returns the response byte.
    //
    // A device-select byte (0x01 pad / 0x81 card) takes effect only when NO
    // transaction is in flight on the armed slot (re-arming the same kind or
    // switching kinds); while a transaction IS in flight it is ordinary
    // payload data — pads and cards both transmit 0x01/0x81 as button or
    // payload bytes.
    uint8_t xfer(uint8_t tx) {
        bits_ += 8;
        const int slot = active_slot();
        if (slot < 0) return 0xFF;  // no slot enabled: bus reads high

        if (tx == kSelectPad || tx == kSelectCard) {
            const Kind k = (tx == kSelectPad) ? Kind::Pad : Kind::Card;
            if (!session_busy()) {
                deselect();
                armed_slot_ = slot;
                armed_kind_ = k;
                if (k == Kind::Pad) {
                    pads[slot].select(true);
                } else {
                    cards[slot].select(true);
                }
                return 0xFF;  // select byte itself draws no data
            }
            // same-kind select during an active session: fall through as data
        }
        if (armed_kind_ == Kind::Pad) return pads[armed_slot_].handle(tx);
        if (armed_kind_ == Kind::Card) return cards[armed_slot_].handle(tx);
        return 0xFF;
    }

    // Drop /SELECT (end of session).
    void deselect() {
        if (armed_kind_ == Kind::Pad && armed_slot_ >= 0)
            pads[armed_slot_].select(false);
        if (armed_kind_ == Kind::Card && armed_slot_ >= 0)
            cards[armed_slot_].select(false);
        armed_slot_ = -1;
        armed_kind_ = Kind::None;
    }

private:
    enum class Kind { None, Pad, Card };

    uint16_t ctrl_ = 0;
    uint16_t mode_ = 0;
    uint16_t baud_ = 0;
    int armed_slot_ = -1;
    Kind armed_kind_ = Kind::None;
    uint64_t bits_ = 0;
};

}  // namespace sio
