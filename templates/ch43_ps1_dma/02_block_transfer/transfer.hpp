#pragma once
//
// ch43 / 02_block_transfer — burst and slice block modes with chopping
// (psx-spx: CHCR sync modes 0/1, chopping window fields).
//
// Cycle model (documented, deterministic — used by goldens):
//   * every moved word costs 1 cycle
//   * a burst adds a fixed 8-cycle arbitration startup
//   * slice mode moves chunks of the DMA-chopping window, then hands the
//     bus back to the CPU for the CPU-window cycles between chunks.
#include <algorithm>
#include <cstdint>
#include <vector>

namespace ps1 {

struct ChannelRegs {
    uint32_t madr = 0;
    uint32_t bcr = 0;   // [31:16] blocks, [15:0] words per block
    uint32_t chcr = 0;
};

inline unsigned bcr_word_count(uint32_t bcr) { return bcr & 0xFFFFu; }
inline unsigned bcr_block_count(uint32_t bcr) { return bcr >> 16; }
inline bool chcr_from_ram(uint32_t chcr) { return (chcr & 1u) != 0; }

enum SyncMode : unsigned { SyncBurst = 0, SyncSlice = 1 };

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Total words moved by one block-mode transfer. BCR counts BLOCKS of
// WORDS; games routinely program e.g. 16 blocks x 16 words for MDEC data.
inline uint32_t total_words(const ChannelRegs& r) {
    return bcr_block_count(r.bcr) * bcr_word_count(r.bcr);
}
//@LABS-STUB
// TODO(1): block count times word count from BCR.
inline uint32_t total_words(const ChannelRegs& r) {
    (void)r;
    return 0;  // wrong on purpose: nothing would transfer
}
//@LABS-END

// 2 MiB main-RAM scratch, word addressed by byte address.
struct Ram {
    Ram() : words_(1u << 19, 0u) {}
    uint32_t read(uint32_t addr) const { return words_[(addr >> 2) & 0x7FFFFu]; }
    void write(uint32_t addr, uint32_t v) { words_[(addr >> 2) & 0x7FFFFu] = v; }
    void fill_pattern(uint32_t base, uint32_t first, uint32_t count) {
        for (uint32_t i = 0; i < count; ++i) write(base + 4 * i, first + i);
    }

private:
    std::vector<uint32_t> words_;
};

// Device-side DMA endpoint. Devices either sink words (RAM -> device,
// CHCR direction bit 0 set) or source them (device -> RAM).
class DmaEndpoint {
public:
    virtual ~DmaEndpoint() = default;
    virtual void write_word(uint32_t) = 0;
    virtual uint32_t read_word() = 0;
};

// Test double: collects everything written, replays a fixed source.
class VectorEndpoint : public DmaEndpoint {
public:
    explicit VectorEndpoint(std::vector<uint32_t> source = {})
        : source_(std::move(source)) {}
    void write_word(uint32_t w) override { sink_.push_back(w); }
    uint32_t read_word() override {
        return index_ < source_.size() ? source_[index_++] : 0xFFFFFFFFu;
    }
    const std::vector<uint32_t>& sink() const { return sink_; }

private:
    std::vector<uint32_t> source_;
    std::vector<uint32_t> sink_;
    size_t index_ = 0;
};

struct TransferResult {
    uint32_t words = 0;
    uint64_t cycles = 0;
};

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Burst mode: one uninterrupted run over the whole block list.
// Direction bit 0 selects RAM->device vs device->RAM.
inline TransferResult run_burst(Ram& ram, DmaEndpoint& dev,
                                const ChannelRegs& r) {
    TransferResult res;
    const uint32_t n = total_words(r);
    const bool to_device = !chcr_from_ram(r.chcr);
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t addr = r.madr + 4 * i;
        if (to_device)
            dev.write_word(ram.read(addr));
        else
            ram.write(addr, dev.read_word());
    res.cycles = uint64_t(n) + 8;  // + arbitration startup
    }
    res.words = n;
    return res;
}
//@LABS-STUB
// TODO(2): move total_words() words between RAM and endpoint honoring the
// direction bit (bit 0 CLEAR means RAM->device). Cycles = words + 8.
inline TransferResult run_burst(Ram& ram, DmaEndpoint& dev,
                                const ChannelRegs& r) {
    (void)ram;
    (void)dev;
    (void)r;
    return {};  // wrong on purpose: transfers nothing
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// Chopping window decoding. Field n encodes (n+1) units: words for the
// DMA side, CPU-regain cycles for the CPU side. A ZERO field disables
// chopping on that side.
inline unsigned dma_window_words(unsigned field) {
    return field == 0 ? 0u : (field + 1u) * 4u;
}
inline unsigned cpu_window_cycles(unsigned field) {
    return field == 0 ? 0u : (field + 1u) * 8u;
}
//@LABS-STUB
// TODO(3): decode both window fields ((n+1)*4 words, (n+1)*8 cycles,
// zero field => disabled => 0).
inline unsigned dma_window_words(unsigned field) {
    (void)field;
    return 0;  // wrong on purpose
}
inline unsigned cpu_window_cycles(unsigned field) {
    (void)field;
    return 0;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// Slice mode with optional chopping: move min(window, remaining) words,
// then yield the bus for the CPU window before the next chunk. With no
// windows programmed this degenerates to a single full-size chunk.
inline TransferResult run_slice(Ram& ram, DmaEndpoint& dev,
                                const ChannelRegs& r,
                                unsigned dma_field, unsigned cpu_field) {
    TransferResult res;
    const uint32_t n = total_words(r);
    const bool to_device = !chcr_from_ram(r.chcr);
    const unsigned win = dma_window_words(dma_field);
    const unsigned cpu_win = cpu_window_cycles(cpu_field);

    uint32_t done = 0;
    while (done < n) {
        const uint32_t chunk = (win == 0) ? (n - done)
                                          : std::min<uint32_t>(win, n - done);
        for (uint32_t k = 0; k < chunk; ++k) {
            const uint32_t addr = r.madr + 4 * (done + k);
            if (to_device)
                dev.write_word(ram.read(addr));
            else
                ram.write(addr, dev.read_word());
        }
        done += chunk;
        res.cycles += chunk;
        if (done < n && cpu_win > 0) res.cycles += cpu_win;
    }
    res.words = n;
    return res;
}
//@LABS-STUB
// TODO(4): chunked loop as documented above. Between chunks (while words
// remain) add cpu_window_cycles to the cycle count. Content must match
// run_burst exactly.
inline TransferResult run_slice(Ram& ram, DmaEndpoint& dev,
                                const ChannelRegs& r,
                                unsigned dma_field, unsigned cpu_field) {
    (void)ram;
    (void)dev;
    (void)r;
    (void)dma_field;
    (void)cpu_field;
    return {};  // wrong on purpose
}
//@LABS-END

}  // namespace ps1

#include <algorithm>
