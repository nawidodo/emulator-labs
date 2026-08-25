#pragma once
//
// ch43 / 99_coding_test — chain inspector (unseen-spec coding test).
// The specification lives in CODING_TEST.md next to this file. Implement
// it EXACTLY; hidden grading runs your build against chains you have
// never seen, including boundary edge cases.

#include <cstdint>
#include <vector>

namespace ps1ct {

constexpr uint32_t kTerminator = 0x00FFFFFFu;

enum class ChainStatus {
    Ok,                 // walked into the exact terminator
    MissingTerminator,  // safety cap hit without a terminator
    PointerOutOfRange,  // link points past the RAM window
    SelfLoop,           // packet links to its own header address
};

struct Ram {
    Ram() : words_(1024, 0u) {}
    size_t word_slots() const { return words_.size(); }
    uint32_t read(uint32_t addr) const { return words_[(addr >> 2) & 0x3FFu]; }
    void write(uint32_t addr, uint32_t v) {
        words_[(addr >> 2) & 0x3FFu] = v;
    }

private:
    std::vector<uint32_t> words_;
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline ChainStatus inspect_chain(const Ram& ram, uint32_t madr,
                                 uint32_t max_packets = 256) {
    if ((madr & 3u) != 0) return ChainStatus::PointerOutOfRange;
    if (madr >= ram.word_slots() * 4) return ChainStatus::PointerOutOfRange;

    uint32_t addr = madr;
    for (uint32_t i = 0; i < max_packets; ++i) {
        const uint32_t hdr = ram.read(addr);
        const uint32_t next = hdr & kTerminator;
        if (next == kTerminator) return ChainStatus::Ok;
        if (next == addr) return ChainStatus::SelfLoop;
        if ((next & 3u) != 0 || next >= ram.word_slots() * 4)
            return ChainStatus::PointerOutOfRange;
        addr = next;
    }
    return ChainStatus::MissingTerminator;
}
//@LABS-STUB
// TODO(1): implement the walk per CODING_TEST.md. The stub accepts
// everything — wrong on purpose.
inline ChainStatus inspect_chain(const Ram& ram, uint32_t madr,
                                 uint32_t max_packets = 256) {
    (void)ram;
    (void)madr;
    (void)max_packets;
    return ChainStatus::Ok;
}
//@LABS-END

inline const char* status_name(ChainStatus s) {
    switch (s) {
        case ChainStatus::Ok: return "Ok";
        case ChainStatus::MissingTerminator: return "MissingTerminator";
        case ChainStatus::PointerOutOfRange: return "PointerOutOfRange";
        case ChainStatus::SelfLoop: return "SelfLoop";
    }
    return "?";
}

}  // namespace ps1ct
