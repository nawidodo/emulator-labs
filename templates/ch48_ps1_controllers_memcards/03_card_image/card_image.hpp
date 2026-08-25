#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <string>

#include "../02_card_protocol/memcard.hpp"

namespace sio {

// ---------------------------------------------------------------------------
// Memory-card IMAGE: the on-disk .mcr bytes plus the directory scheme this
// course uses for bad-sector flags.
//
// .mcr layout (131072 bytes = 1024 sectors x 128):
//   sector 0            : header. Bytes 0..1 = "MC" magic, rest 0xFF.
//   sectors 1..15       : directory entries, entry i <-> data block i.
//   blocks 1..15        : data, block b covers sectors [b*64, b*64+63).
//
// Directory entry (128 bytes at sector i, 1 <= i <= 15):
//   byte 0      : state — 0xA0 in use, 0x51 deleted, 0xFF free
//   bytes 2..3  : size in blocks, little-endian
//   bytes 4..19 : 16-char ASCII name, zero padded
//   byte 21     : attribute — bit 7 set = THIS BLOCK IS BAD (course model:
//                 real cards keep per-sector fault maps in vendor areas; we
//                 flag whole blocks through one directory bit so behavior is
//                 deterministic and testable)
//   all others  : 0xFF
// ---------------------------------------------------------------------------

class CardImage {
public:
    static constexpr unsigned kSize = kImageBytes;
    static constexpr uint8_t kStateInUse = 0xA0;
    static constexpr uint8_t kStateDeleted = 0x51;
    static constexpr uint8_t kStateFree = 0xFF;
    static constexpr uint8_t kAttrBadBlock = 0x80;

    void reset() {
        bytes_.fill(0xFF);
        bytes_[0] = 'M';
        bytes_[1] = 'C';
    }

    const std::array<uint8_t, kImageBytes>& bytes() const { return bytes_; }
    std::array<uint8_t, kImageBytes>& bytes() { return bytes_; }

    // Parse the directory and mark bad sectors on `card`.
    void mount_into(MemCard& card) const {
        card.reset();
        card.load_image(bytes_.data());
        for (unsigned e = 1; e < kBlockCount && e < 16; ++e) {
            const uint8_t* ent = sector(e);
            // Only LIVE entries are consulted; free entries are all-0xFF and
            // their attribute byte must not be mistaken for a bad flag.
            if (ent[0] != kStateInUse && ent[0] != kStateDeleted) continue;
            if (ent[21] & kAttrBadBlock) mark_block_bad(card, e, true);
        }
    }

    void mark_block_bad(MemCard& card, unsigned block, bool bad) const {
        if (block == 0 || block >= kBlockCount) return;
        for (unsigned s = block * kBlockSectors;
             s < (block + 1) * kBlockSectors; ++s) {
            card.set_bad(s, bad);
        }
    }

    // Write a directory entry for block `block` into the image bytes.
    void set_entry(unsigned block, uint8_t state, uint16_t size_blocks,
                   const std::string& name, bool bad_block) {
        if (block < 1 || block >= kBlockCount || name.size() > 16) return;
        uint8_t* ent = sector(block);
        std::fill(ent, ent + kSectorSize, 0xFF);
        ent[0] = state;
        ent[2] = static_cast<uint8_t>(size_blocks & 0xFF);
        ent[3] = static_cast<uint8_t>(size_blocks >> 8);
        for (unsigned i = 0; i < name.size(); ++i) ent[4 + i] =
            static_cast<uint8_t>(name[i]);
        ent[21] = bad_block ? kAttrBadBlock : 0x00;
    }

    const uint8_t* entry(unsigned block) const { return sector(block); }

private:
    const uint8_t* sector(unsigned s) const {
        return bytes_.data() + s * kSectorSize;
    }
    uint8_t* sector(unsigned s) { return bytes_.data() + s * kSectorSize; }

    std::array<uint8_t, kImageBytes> bytes_;
};

}  // namespace sio
