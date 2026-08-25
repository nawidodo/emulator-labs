#pragma once
#include <array>
#include <cstdint>
#include <cstring>

// Debugging variant of 02_card_protocol/memcard.hpp (namespace siodbg)
// carrying SEEDED BUG: the XOR checksum is computed over the WRONG RANGE.
// The @LABS SOLUTION side below is the correct reference; the STUB side
// carries the bug. Only the checksum seam differs from the reference card;
// everything else mirrors memcard.hpp exactly.
#include "../01_digital_pad/pad.hpp"
#include "../02_card_protocol/memcard.hpp"

namespace siodbg {

using sio::CMD_ERASE;
using sio::CMD_GETID;
using sio::CMD_READ;
using sio::CMD_WRITE;
using sio::kCardId;
using sio::kFlagBad;
using sio::kFlagGood;
using sio::kFlagMid;
using sio::kFlagPre;
using sio::kImageBytes;
using sio::kSectorCount;
using sio::kSectorSize;
using sio::kSelectCard;

class MemCard {
public:
    bool selected() const { return selected_; }

    void reset() {
        selected_ = false;
        phase_ = Phase::Idle;
        bad_.fill(false);
    }

    void select(bool low) {
        // falling edge arms the card (next byte is the command); rising
        // edge ends the transaction — mirrors memcard.hpp
        phase_ = low ? Phase::Cmd : Phase::Idle;
        await_cmd_ = low;
        selected_ = low;
    }

    void load_image(const uint8_t* img) {
        std::memcpy(store_.data(), img, kImageBytes);
    }

    uint8_t* sector_data(unsigned s) { return store_[s].data(); }
    const uint8_t* sector_data(unsigned s) const { return store_[s].data(); }
    void set_bad(unsigned s, bool bad) { bad_[s] = bad; }
    bool is_bad(unsigned s) const { return bad_[s]; }

//@LABS-BEGIN 1
//@LABS-SOLUTION
    // Correct XOR: over EXACTLY n payload bytes.
    uint8_t xor_checksum(const uint8_t* p, unsigned n) {
        uint8_t x = 0;
        for (unsigned i = 0; i < n; ++i) x = static_cast<uint8_t>(x ^ p[i]);
        return x;
    }
//@LABS-STUB
    // TODO(1): BUG SEEDED — this reduces only p[0..n-2], silently dropping
    // the LAST payload byte from the sum (and reading one byte short on
    // GETID). Fix it to cover all n bytes.
    uint8_t xor_checksum(const uint8_t* p, unsigned n) {
        uint8_t x = 0;
        for (unsigned i = 0; i + 1 < n; ++i)
            x = static_cast<uint8_t>(x ^ p[i]);
        return x;
    }
//@LABS-END

    // True while a transaction is in flight (command/address/data phases).
    bool busy() const {
        return selected_ && phase_ != Phase::Idle &&
               phase_ != Phase::Done && phase_ != Phase::Dead;
    }

    // Clock one transmitted byte in, get the response byte out. When driven
    // WITHOUT the bus wrapper the 0x81 device-select byte arrives here too:
    // it draws no data and just re-arms the command phase.
    uint8_t handle(uint8_t tx) {
        if (!selected_) return 0xFF;
        // A literal 0x81 is a device-select ONLY between transactions;
        // inside one it is ordinary payload data.
        if (await_cmd_) {
            if (tx == kSelectCard) return 0xFF;
            await_cmd_ = false;
        }
        switch (phase_) {
            case Phase::Cmd: {
                cmd_ = tx;
                count_ = 0;
                switch (tx) {
                    case CMD_READ:
                    case CMD_WRITE:
                    case CMD_GETID:
                    case CMD_ERASE:
                        phase_ = Phase::Addr;
                        return kFlagPre;
                    default:
                        phase_ = Phase::Dead;
                        return 0xFF;
                }
            }
            case Phase::Addr: {
                addr_[count_++] = tx;
                if (count_ < 3) return kFlagMid;
                sector_ =
                    static_cast<unsigned>(((addr_[0] << 8) | addr_[1]) & 0x3FF);
                switch (cmd_) {
                    case CMD_READ:
                    case CMD_GETID: phase_ = Phase::Tx; break;
                    case CMD_WRITE: phase_ = Phase::Rx; break;
                    default: phase_ = Phase::Tail; break;
                }
                count_ = 0;
                return kFlagMid;
            }
            case Phase::Tx: {
                const unsigned len = (cmd_ == CMD_GETID) ? 4 : 128;
                const unsigned i = count_++;
                if (i < len)
                    return is_bad(sector_) ? 0xFF : store_[sector_][i];
                if (i == len) {
                    if (is_bad(sector_)) return 0x00;
                    if (cmd_ == CMD_GETID)
                        return xor_checksum(kCardId.data(), 4);
                    return xor_checksum(store_[sector_].data(), kSectorSize);
                }
                phase_ = Phase::Done;
                return is_bad(sector_) ? kFlagBad : kFlagGood;
            }
            case Phase::Rx: {
                const unsigned i = count_++;
                if (i < 128) buf_[i] = tx;
                else chk_ = tx;
                if (count_ == 129) phase_ = Phase::Tail;
                return kFlagMid;
            }
            case Phase::Tail: {
                phase_ = Phase::Done;
                if (is_bad(sector_)) return kFlagBad;
                if (cmd_ == CMD_ERASE) {
                    store_[sector_].fill(0xFF);
                    return kFlagGood;
                }
                if (chk_ != xor_checksum(buf_.data(), kSectorSize))
                    return kFlagBad;
                std::memcpy(store_[sector_].data(), buf_.data(), kSectorSize);
                return kFlagGood;
            }
            default: return 0xFF;  // Idle / Done / Dead
        }
    }

private:
    enum class Phase { Idle, Cmd, Addr, Tx, Rx, Tail, Done, Dead };

    Phase phase_ = Phase::Idle;
    bool selected_ = false;
    bool await_cmd_ = false;
    uint8_t cmd_ = 0;
    unsigned count_ = 0;
    unsigned sector_ = 0;
    std::array<uint8_t, 3> addr_{};
    std::array<uint8_t, 128> buf_{};
    uint8_t chk_ = 0;
    std::array<std::array<uint8_t, kSectorSize>, kSectorCount> store_{};
    std::array<bool, kSectorCount> bad_{};
};

}  // namespace siodbg
