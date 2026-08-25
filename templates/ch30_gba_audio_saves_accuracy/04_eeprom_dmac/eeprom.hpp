#pragma once
// GBA EEPROM: bit-banged serial protocol as driven by DMA3 halfword
// streams (bit 15 of each u16 carries one bit).
//
// Frame layout after the START bit:
//   RD: "10" + addr(width bits) + stop "0"   -> device answers 64 bits
//   WR: "00" + addr(width bits) + 64 data + "0"
#include <cstdint>
#include <vector>

namespace gba {

using u8 = uint8_t;
using s8 = int8_t;
using u16 = uint16_t;
using s16 = int16_t;
using u32 = uint32_t;
using s32 = int32_t;
using u64 = uint64_t;

constexpr int kEeprom512B = 512;
constexpr int kEeprom8KB = 8 * 1024;

struct Eeprom {
    int size = kEeprom512B;         // device capacity in bytes
    std::vector<u8> mem;
    std::vector<u8> out_bits;       // queued response bits for reads
    bool last_word = 0;

    enum class State { Idle, Op, Addr, WriteData, ReadOut };
    State state = State::Idle;
    int op = -1;                    // 2 = read ("10"), 0 = write ("00")
    u32 addr = 0;
    int bits_left = 0;

    explicit Eeprom(int bytes) : size(bytes), mem(size, 0xFF) {}

    int addr_width() const { return size == kEeprom512B ? 6 : 14; }

//@LABS-BEGIN 1
//@LABS-SOLUTION
    // Feed one protocol bit from the game side.
    void feed(int bit) {
        switch (state) {
            case State::Idle:
                if (bit == 1) {
                    state = State::Op;
                    op = 0;
                    bits_left = 2;
                }
                break;
            case State::Op:
                op = (op << 1) | (bit & 1);
                if (--bits_left == 0) {
                    state = State::Addr;
                    addr = 0;
                    bits_left = addr_width();
                }
                break;
            case State::Addr:
                addr = (addr << 1) | u32(bit & 1);
                if (--bits_left == 0) {
                    if (op == 2) {  // read: queue 64 response bits
                        state = State::ReadOut;
                        queue_read_response();
                    } else {
                        state = State::WriteData;
                        bits_left = 64;
                    }
                }
                break;
            case State::WriteData: {
                // MSB-first into a 64-bit little-endian window of memory.
                int byte = 64 - bits_left;
                int idx = int(addr) * 64 + byte;   // bit address of the cell
                if (idx < size * 8) {
                    u8 mask = u8(1 << (byte % 8));
                    if (bit)
                        mem[idx / 8] |= mask;
                    else
                        mem[idx / 8] &= u8(~mask);
                }
                if (--bits_left == 0)
                    state = State::Idle;  // stop bit handled by stop()
                break;
            }
            default:
                break;
        }
        (void)last_word;
    }

    // Consume the trailing stop bit after writes/reads (no-op logically,
    // kept for protocol completeness).
    void stop() {
        if (state == State::WriteData || state == State::ReadOut) {
            state = State::Idle;
            out_bits.clear();
        }
    }
//@LABS-STUB
    // TODO(1): implement the bit-level state machine per LECTURE.md:
    // Idle --1--> two op bits ("10" read / "00" write), then the address
    // (6 bits for 512 B, 14 for 8 KB), then 64 data bits on write (MSB
    // first) or a queued 64-bit response on read. `stop()` returns to Idle.
    void feed(int) {}
    void stop() {}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
    void queue_read_response() {
        out_bits.clear();
        for (int i = 63; i >= 0; --i) {
            int idx = int(addr) * 64 + (63 - i);
            u8 v = idx < size * 8 ? u8((mem[idx / 8] >> (idx % 8)) & 1) : 0;
            out_bits.push_back(v);
        }
    }

    // Pop one answer bit during ReadOut (0 when exhausted).
    int read_bit() {
        if (out_bits.empty()) return 0;
        int b = out_bits.front();
        out_bits.erase(out_bits.begin());
        return b;
    }
//@LABS-STUB
    // TODO(2): queue the 64 answer bits MSB-first from the addressed word
    // and serve them one at a time via read_bit().
    void queue_read_response() {}
    int read_bit() { return 0; }  // wrong on purpose
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
    // Convenience: feed a DMA3-style stream — bit 15 of each u16, in order.
    void feed_dma_stream(const u16* words, int n) {
        for (int i = 0; i < n; ++i)
            for (int b = 15; b >= 0; --b) feed((words[i] >> b) & 1);
    }
//@LABS-STUB
    // TODO(3): split each halfword MSB-first and feed its 16 bits.
    void feed_dma_stream(const u16*, int) {}
//@LABS-END
};

}  // namespace gba
