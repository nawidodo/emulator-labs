#pragma once
//
// ch43 / 90_debug — SEEDED BUGS in a linked-list GPU walker.
//
// Two independent bugs are present on the STUB side. The unit tests below
// fail RED until you find and fix both. Write up your findings in
// bug-report.md (bug / root cause / first divergence / fix / regression).
//
// Test RAM layout used by the tests (word addresses):
//   0x000 hdr count=2 next=0x010
//   0x010 w 0x11111111
//   0x014 w 0x22222222
//   0x018 hdr count=1 next=0x030   <- final packet, terminator link
//   ...stale garbage at 0x030.. beyond the chain

#include <cstdint>
#include <vector>

namespace ps1dbg {

constexpr uint32_t kTerminator = 0x00FFFFFFu;

struct Ram {
    Ram() : words_(256, kDeadPattern) {}
    static constexpr uint32_t kDeadPattern = 0xDEADBEEFu;
    uint32_t read(uint32_t addr) const { return words_[(addr >> 2) & 0xFFu]; }
    void write(uint32_t addr, uint32_t v) { words_[(addr >> 2) & 0xFFu] = v; }

private:
    std::vector<uint32_t> words_;
};

struct Sink {
    std::vector<uint32_t> got;
    void push(uint32_t w) { got.push_back(w); }
};

struct Result {
    uint32_t packets = 0;
    bool terminated = false;
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Termination rule: a link ends the chain IFF it is exactly 0FFFFFFh.
inline bool link_ends(uint32_t next_link) {
    return next_link == kTerminator;
}
//@LABS-STUB
// TODO(1): the check below accepts the WRONG sentinel value. Real chains
// end with 0FFFFFFh; decide what this code actually stops on and fix it.
inline bool link_ends(uint32_t next_link) {
    return next_link == 0x00000000u;  // BUG 1: wrong sentinel
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Payload word count: header bits [31:24] only. The header itself belongs
// to the DMA unit and must never reach the GPU.
inline uint32_t payload_words(uint32_t hdr) {
    return hdr >> 24;
}
//@LABS-STUB
// TODO(2): symptom — every packet delivers one word too many and the
// captured stream is shifted by one word per packet.
inline uint32_t payload_words(uint32_t hdr) {
    return (hdr >> 24) + 1;  // BUG 2: counts the header as payload
}
//@LABS-END

inline Result walk(Ram& ram, uint32_t madr, Sink& sink,
                   uint32_t max_packets = 64) {
    Result r;
    uint32_t addr = madr;
    while (true) {
        const uint32_t hdr = ram.read(addr);
        const uint32_t next = hdr & kTerminator;
        const uint32_t n = payload_words(hdr);
        for (uint32_t k = 0; k < n; ++k)
            sink.push(ram.read(addr + 4 + 4 * k));
        ++r.packets;
        if (link_ends(next)) {
            r.terminated = true;
            break;
        }
        if (r.packets >= max_packets) break;  // safety net
        addr = next;
    }
    return r;
}

}  // namespace ps1dbg
