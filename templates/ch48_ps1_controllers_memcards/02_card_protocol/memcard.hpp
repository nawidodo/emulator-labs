#pragma once
#include <array>
#include <cstdint>
#include <cstring>

namespace sio {

// ---------------------------------------------------------------------------
// Memory card protocol (this course's model).
//
// Geometry: a standard headerless .mcr image is 131072 bytes = 1024 sectors
// of 128 bytes. The curriculum's "blocks" phrasing maps onto the file layout
// of 16 blocks x 64 sectors x 128 bytes (8 KiB per block); see
// 03_card_image/card_image.hpp for the directory scheme.
//
// Transaction framing (byte-per-byte over 1F801040):
//
//   READ   tx: 81 | 52 | a0 | a1 | a2 | 00 * 129 | 00
//          rx: FF | 5D | 5C  5C  5C | d0..d127 | chk | 5A
//   WRITE  tx: 81 | 57 | a0 | a1 | a2 | d0..d127 | chk | 00
//          rx: FF | 5D | 5C  5C  5C | 5D * 129       | 5A
//   GETID  tx: 81 | 53 | 00 | 00 | 00 | 00 * 4    | 00 | 00
//          rx: FF | 5D | 5C  5C  5C | 04 00 00 80  | 84 | 5A
//   ERASE  tx: 81 | 43 | a0 | a1 | a2 | 00
//          rx: FF | 5D | 5C  5C  5C | 5A
//
// Flag bytes: 0x5D = pre (response to the command byte), 0x5C = mid (address
// phase and write data phase), 0x5A = good end. A sector flagged BAD in the
// directory answers payload of all-0xFF with checksum 0x00 (XOR of 128 x FF)
// and ends with 0xFF instead of 0x5A; writes to it are discarded.
//
// Addressing: three bytes MSB first. Our model computes
// sector = ((a0 << 8) | a1) & 0x3FF and IGNORES a2 (must be sent, may be 0).
// 10 bits cover exactly the 1024 sectors.
//
// Checksum: XOR over the 128 payload bytes only (never over flag/address
// bytes). GETID payload is the fixed ID 04 00 00 80 -> checksum 0x84.
// ---------------------------------------------------------------------------

constexpr unsigned kSectorSize = 128;
constexpr unsigned kSectorCount = 1024;
constexpr unsigned kImageBytes = kSectorSize * kSectorCount;  // 131072
constexpr unsigned kBlockSectors = 64;                        // one "block"
constexpr unsigned kBlockCount = kSectorCount / kBlockSectors;  // 16

constexpr uint8_t kSelectCard = 0x81;
enum CardCmd : uint8_t {
    CMD_READ = 0x52,
    CMD_WRITE = 0x57,
    CMD_GETID = 0x53,
    CMD_ERASE = 0x43,
};
constexpr uint8_t kFlagPre = 0x5D;
constexpr uint8_t kFlagMid = 0x5C;
constexpr uint8_t kFlagGood = 0x5A;
constexpr uint8_t kFlagBad = 0xFF;
inline constexpr std::array<uint8_t, 4> kCardId{0x04, 0x00, 0x00, 0x80};

//@LABS-BEGIN 1
//@LABS-SOLUTION
// XOR over exactly n payload bytes. n is ALWAYS 128 here (or 4 for GETID);
// flag and address bytes never enter the sum.
inline uint8_t xor_checksum(const uint8_t* p, unsigned n) {
    uint8_t x = 0;
    for (unsigned i = 0; i < n; ++i) x = static_cast<uint8_t>(x ^ p[i]);
    return x;
}
//@LABS-STUB
// TODO(1): XOR-reduce p[0..n-1]. Return 0 for now so the suite compiles RED.
inline uint8_t xor_checksum(const uint8_t*, unsigned) {
    return 0;
}
//@LABS-END

class MemCard {
public:
    bool selected() const { return selected_; }

    void reset() {
        selected_ = false;
        phase_ = Phase::Idle;
        last_final_ = true;
        bad_.fill(false);
    }
    // /SELECT line: falling edge (select) ARMS the card — the next byte it
    // sees is the command. Rising edge ends any partial transaction. When
    // driven without the bus wrapper, handle() also re-arms on a literal
    // 0x81 device-select byte.
    void select(bool low) {
        if (!low) {
            phase_ = Phase::Idle;
            last_final_ = true;
        } else {
            phase_ = Phase::Cmd;
            await_cmd_ = true;  // next byte must be the command
        }
        selected_ = low;
    }

    void load_image(const uint8_t* img) {
        std::memcpy(store_.data(), img, kImageBytes);
    }

    void export_image(uint8_t* img) const {
        std::memcpy(img, store_.data(), kImageBytes);
    }

    uint8_t* sector_data(unsigned s) { return store_[s].data(); }
    const uint8_t* sector_data(unsigned s) const { return store_[s].data(); }

    void set_bad(unsigned s, bool bad) { bad_[s] = bad; }
    bool is_bad(unsigned s) const { return bad_[s]; }

//@LABS-BEGIN 2
//@LABS-SOLUTION
    // Command byte phase: known commands draw the PRE flag (0x5D), unknown
    // ones kill the session until deselect (0xFF forever).
    uint8_t cmd_phase(uint8_t tx) {
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
                last_final_ = true;
                return 0xFF;
        }
    }
//@LABS-STUB
    // TODO(2): latch the command; reply kFlagPre for the four known commands
    // and park in Dead (reply 0xFF) otherwise. Return 0xFF for now.
    uint8_t cmd_phase(uint8_t) {
        return 0xFF;
    }
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
    // Address phase: three bytes MSB first. sector = ((a0<<8)|a1) & 0x3FF;
    // a2 is clocked but ignored. Every address byte draws the MID flag.
    uint8_t addr_phase(uint8_t tx) {
        addr_[count_++] = tx;
        if (count_ < 3) return kFlagMid;
        sector_ = static_cast<unsigned>(((addr_[0] << 8) | addr_[1]) & 0x3FF);
        switch (cmd_) {
            case CMD_READ:
            case CMD_GETID:
                phase_ = Phase::Tx;
                break;
            case CMD_WRITE:
                phase_ = Phase::Rx;
                break;
            default:
                phase_ = Phase::Tail;  // erase completes on the next clock
                break;
        }
        count_ = 0;
        return kFlagMid;
    }
//@LABS-STUB
    // TODO(3): collect the 3 MSB-first address bytes, derive sector_ =
    // ((a0<<8)|a1)&0x3FF, move to Tx/Rx/Tail per command, answer kFlagMid.
    // Return 0xFF for now.
    uint8_t addr_phase(uint8_t) {
        return 0xFF;
    }
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
    // Read/GETID data phase: len payload bytes, then the checksum byte, then
    // the end flag replaces what would be one more payload byte. A BAD sector
    // streams all-0xFF payload with checksum 0x00 and ends 0xFF.
    uint8_t tx_phase() {
        const unsigned len = (cmd_ == CMD_GETID) ? 4 : 128;
        const unsigned i = count_++;
        if (i < len) {  // payload byte
            if (is_bad(sector_)) return 0xFF;
            return (cmd_ == CMD_GETID) ? kCardId[i] : store_[sector_][i];
        }
        if (i == len) {  // checksum position
            if (is_bad(sector_)) return 0x00;
            if (cmd_ == CMD_GETID) return xor_checksum(kCardId.data(), 4);
            return xor_checksum(store_[sector_].data(), kSectorSize);
        }
        phase_ = Phase::Done;  // i == len+1: end-flag position
        last_final_ = true;
        return is_bad(sector_) ? kFlagBad : kFlagGood;
    }
//@LABS-STUB
    // TODO(4): stream payload + checksum + end flag as specified above,
    // honoring the bad-sector rule (payload 0xFF, checksum 0x00, end 0xFF).
    // Return 0xFF for now.
    uint8_t tx_phase() {
        return 0xFF;
    }
//@LABS-END

//@LABS-BEGIN 5
//@LABS-SOLUTION
    // Write data phase: swallow 128 data bytes + checksum, replying MID each.
    uint8_t rx_phase(uint8_t tx) {
        const unsigned i = count_++;
        if (i < 128) {
            buf_[i] = tx;
        } else {
            chk_ = tx;
        }
        if (count_ == 129) phase_ = Phase::Tail;  // one more clock: end flag
        return kFlagMid;
    }

    // One extra clock after the data: validate and finish. Erase fills the
    // sector with 0xFF. A write commits only if the checksum matches AND the
    // sector is not flagged bad; otherwise nothing is written.
    uint8_t tail_phase() {
        phase_ = Phase::Done;
        last_final_ = true;
        if (is_bad(sector_)) return kFlagBad;
        if (cmd_ == CMD_ERASE) {
            store_[sector_].fill(0xFF);
            return kFlagGood;
        }
        if (chk_ != xor_checksum(buf_.data(), kSectorSize)) return kFlagBad;
        store_[sector_] = buf_;
        return kFlagGood;
    }
//@LABS-STUB
    // TODO(5): implement rx_phase + tail_phase exactly as described above
    // (both functions live in this one block). Reply kFlagGood
    // unconditionally for now so the suite compiles RED.
    uint8_t rx_phase(uint8_t) {
        return kFlagMid;
    }
    uint8_t tail_phase() {
        return kFlagGood;
    }
//@LABS-END

    // Clock one transmitted byte in, get the response byte out. When driven
    // WITHOUT the bus wrapper the 0x81 device-select byte arrives here too:
    // it draws no data and just re-arms the command phase.
    uint8_t handle(uint8_t tx) {
        last_final_ = false;
        if (!selected_) {
            last_final_ = true;
            return 0xFF;
        }
        // A literal 0x81 before the command byte is the device select the
        // bus would have eaten: no data response. Mid-transaction it is
        // ordinary payload data.
        if (await_cmd_) {
            if (tx == kSelectCard) {
                last_final_ = true;
                return 0xFF;
            }
            await_cmd_ = false;
        }
        switch (phase_) {
            case Phase::Cmd: return cmd_phase(tx);
            case Phase::Addr: return addr_phase(tx);
            case Phase::Tx: return tx_phase();
            case Phase::Rx: return rx_phase(tx);
            case Phase::Tail: return tail_phase();
            default:
                last_final_ = true;
                return 0xFF;  // Idle / Done / Dead
        }
    }

    // True while a transaction is in flight (command/address/data phases).
    bool busy() const {
        return selected_ && phase_ != Phase::Idle &&
               phase_ != Phase::Done && phase_ != Phase::Dead;
    }

    // ACK mirrors "more transaction left": asserted after every non-final
    // response inside an armed session (same convention as the pad).
    bool ack() const {
        return selected_ &&
               (phase_ == Phase::Addr || phase_ == Phase::Tx ||
                (phase_ == Phase::Rx && count_ <= 128));
    }

private:
    enum class Phase { Idle, Cmd, Addr, Tx, Rx, Tail, Done, Dead };

    Phase phase_ = Phase::Idle;
    bool selected_ = false;
    bool await_cmd_ = false;
    bool last_final_ = true;
    uint8_t cmd_ = 0;
    unsigned count_ = 0;
    unsigned sector_ = 0;
    std::array<uint8_t, 3> addr_{};
    std::array<uint8_t, 128> buf_{};
    uint8_t chk_ = 0;
    std::array<std::array<uint8_t, kSectorSize>, kSectorCount> store_{};
    std::array<bool, kSectorCount> bad_{};
};

}  // namespace sio
