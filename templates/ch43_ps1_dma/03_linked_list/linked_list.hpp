#pragma once
//
// ch43 / 03_linked_list — OTC ordering tables and GPU linked-list DMA
// (psx-spx: channel 6 OTC, CHCR sync mode 2, packet headers).
//
// Linked-list semantics that matter:
//   * a packet header word is [31:24] payload word count, [23:0] next link
//   * ONLY an exact 0FFFFFFh link terminates the chain — anything else,
//     including 0, is a hop to another address
//   * the GPU receives exactly `count` payload words per packet; the
//     header itself is consumed by the DMA unit, never by the device.

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace ps1 {

constexpr uint32_t kListTerminator = 0x00FFFFFFu;

struct Ram {
    Ram() : words_(1u << 16, 0u) {}  // 256 KiW test window
    size_t word_slots() const { return words_.size(); }
    uint32_t read(uint32_t addr) const {
        return words_[(addr >> 2) & 0xFFFFu];
    }
    void write(uint32_t addr, uint32_t v) { words_[(addr >> 2) & 0xFFFFu] = v; }

private:
    std::vector<uint32_t> words_;
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
// OTC (channel 6) builds a backwards-linked ordering table: entry at
// `start` points to start-4, and so on; the LAST entry is the exact
// sentinel 0FFFFFFh. Games feed these tables straight into the GPU list
// walker in reverse draw order.
inline void otc_build(Ram& ram, uint32_t start, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t addr = start - 4 * i;
        ram.write(addr, i + 1 < count ? addr - 4 : kListTerminator);
    }
}
//@LABS-STUB
// TODO(1): write `count` descending entries starting at `start`; every
// entry holds its own address minus 4, the final entry holds 0FFFFFFh.
inline void otc_build(Ram& ram, uint32_t start, uint32_t count) {
    (void)ram;
    (void)start;
    (void)count;
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Packet-header decode. The high byte is the PAYLOAD word count (the
// header itself is not delivered to the GPU).
inline uint32_t packet_word_count(uint32_t hdr) {
    return hdr >> 24;
}
inline uint32_t packet_next_link(uint32_t hdr) {
    return hdr & kListTerminator;
}
//@LABS-STUB
// TODO(2): decode payload word count ([31:24]) and next link ([23:0]).
// Watch out: the header word itself is NOT part of the payload.
inline uint32_t packet_word_count(uint32_t hdr) {
    (void)hdr;
    return 0;  // wrong on purpose
}
inline uint32_t packet_next_link(uint32_t hdr) {
    (void)hdr;
    return 0;  // wrong on purpose
}
//@LABS-END

// Device sink for walker output.
struct GpuSink {
    virtual ~GpuSink() = default;
    virtual void push_packet_word(uint32_t) = 0;
};

class CaptureSink : public GpuSink {
public:
    void push_packet_word(uint32_t w) override { words_.push_back(w); }
    const std::vector<uint32_t>& words() const { return words_; }

private:
    std::vector<uint32_t> words_;
};

struct WalkResult {
    uint32_t packets = 0;
    uint32_t words = 0;
    uint64_t cycles = 0;
    bool terminated = false;  // ended on the exact 0FFFFFFh link
    bool cap_hit = false;     // safety cap tripped on malformed chain
};

using TraceLog = std::vector<std::string>;

// Cycle accounting (documented, deterministic):
//   1 cycle per header fetch, 1 per payload word, 1 per pointer hop.
//@LABS-BEGIN 3
//@LABS-SOLUTION
inline WalkResult walk_gpu_list(Ram& ram, uint32_t madr, GpuSink& gpu,
                                TraceLog* trace = nullptr,
                                uint32_t max_packets = 4096) {
    WalkResult r;
    uint32_t addr = madr;
    while (true) {
        const uint32_t hdr = ram.read(addr);
        const uint32_t count = packet_word_count(hdr);
        const uint32_t next = packet_next_link(hdr);

        for (uint32_t k = 0; k < count; ++k)
            gpu.push_packet_word(ram.read(addr + 4 + 4 * k));

        r.cycles += 1 + count;  // header fetch + payload words
        r.words += count;
        if (trace) {
            char buf[80];
            std::snprintf(buf, sizeof(buf),
                          "pkt=%u ptr=%06X hdr=%08X words=%u cyc=%llu",
                          r.packets, addr, hdr, count,
                          static_cast<unsigned long long>(r.cycles));
            trace->emplace_back(buf);
        }
        ++r.packets;

        if (next == kListTerminator) {  // termination-on-last-link-exact
            r.terminated = true;
            break;
        }
        if (r.packets >= max_packets) {
            r.cap_hit = true;
            break;
        }
        addr = next;
        r.cycles += 1;  // pointer hop
    }
    return r;
}
//@LABS-STUB
// TODO(3): walk packets from `madr`: decode each header, deliver EXACTLY
// packet_word_count(hdr) payload words to the GPU, stop only when the low
// 24 bits equal 0FFFFFFh (set .terminated). Respect max_packets by setting
// .cap_hit. Accumulate cycles as documented above and append one trace
// line per packet when `trace` != nullptr.
inline WalkResult walk_gpu_list(Ram& ram, uint32_t madr, GpuSink& gpu,
                                TraceLog* trace = nullptr,
                                uint32_t max_packets = 4096) {
    (void)ram;
    (void)madr;
    (void)gpu;
    (void)trace;
    (void)max_packets;
    return {};  // wrong on purpose: walks nothing
}
//@LABS-END

}  // namespace ps1
