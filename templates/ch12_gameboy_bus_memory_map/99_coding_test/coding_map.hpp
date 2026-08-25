// coding_map.hpp — Tetra-8 map for the unseen-spec coding test.
//
// The full hardware spec lives in CODING_TEST.md next to this file; it
// is authoritative. Same Map interface shape as the challenge and the
// exercises — only the rules differ.
//
// Three TODO blocks. Everything else is provided scaffolding.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace t8 {

// Tetra-8 idles its data bus LOW: dead slots read NUL ($00), never $FF.
constexpr uint8_t kNullBus = 0x00;

constexpr uint16_t kBankSize = 0x2000;       // 4000-5FFF window = one bank
constexpr uint16_t kShadowBase = 0xD000;     // D000-D5FF shadows A000-A5FF
constexpr uint16_t kScratchMirrorDelta = 0x0800;  // E800-EFFF mirrors E000-E7FF

class Map {
public:
    virtual ~Map() = default;
    virtual uint8_t read(uint16_t addr) = 0;
    virtual void write(uint16_t addr, uint8_t val) = 0;
};

class Tetra8Map final : public Map {
public:
    explicit Tetra8Map(const std::vector<uint8_t>& rom) : rom_(&rom) {}

    // ---- test seams --------------------------------------------------
    uint8_t bank() const { return bank_; }
    uint8_t peekRam(size_t off) const { return ram_[off]; }         // 0x000-0xFFF
    uint8_t peekScratch(size_t off) const { return scratch_[off]; } // 0x000-0x7FF

    //@LABS-BEGIN 2
    //@LABS-SOLUTION
    uint8_t read(uint16_t addr) override {
        if (addr <= 0x3FFF) return romByte(addr);                  // fixed bank 0
        if (addr <= 0x5FFF)
            return romByte(static_cast<size_t>(bank_) * kBankSize +
                           (addr - 0x4000));                       // banked window
        if (addr >= 0xA000 && addr <= 0xAFFF)
            return ram_[addr - 0xA000];                            // work RAM
        if (addr >= kShadowBase && addr <= 0xD5FF)
            return ram_[static_cast<uint16_t>(addr - kShadowBase)];  // RAM shadow
        if (addr >= 0xE000 && addr <= 0xE7FF)
            return scratch_[addr - 0xE000];                        // scratchpad
        if (addr >= 0xE800 && addr <= 0xEFFF)
            return scratch_[addr - 0xE800];                        // its mirror
        if (addr == 0xFF00) return bank_;                          // bank register
        return kNullBus;                                           // everything else
    }
    //@LABS-STUB
    uint8_t read(uint16_t addr) override {
        (void)addr;  // TODO(2): full decode per the CODING_TEST.md table
        return kNullBus;
    }
    //@LABS-END

    //@LABS-BEGIN 3
    //@LABS-SOLUTION
    void write(uint16_t addr, uint8_t val) override {
        if (addr <= 0x3FFF || (addr >= 0x6000 && addr <= 0x9FFF)) {
            return;                                   // ROM + closed: dropped
        }
        if (addr >= 0xA000 && addr <= 0xAFFF) ram_[addr - 0xA000] = val;
        else if (addr >= kShadowBase && addr <= 0xD5FF)
            ram_[static_cast<uint16_t>(addr - kShadowBase)] = val;
        else if (addr >= 0xE000 && addr <= 0xE7FF)
            scratch_[addr - 0xE000] = val;
        else if (addr >= 0xE800 && addr <= 0xEFFF)
            scratch_[addr - 0xE800] = val;
        else if (addr == 0xFF00) bank_ = normalizeBank(val);
        // every other address: dropped
    }
    //@LABS-STUB
    void write(uint16_t addr, uint8_t val) override {
        // TODO(3): route writes per spec — RAM, shadow, scratch, mirror,
        // and the FF00 bank register; everything else drops.
        (void)addr;
        (void)val;
    }
    //@LABS-END

private:
    //@LABS-BEGIN 1
    //@LABS-SOLUTION
    // Only two bits are wired; a select of 0 is unwirable and wraps to 1,
    // exactly like classic GB bank latches.
    static uint8_t normalizeBank(uint8_t val) {
        const uint8_t b = static_cast<uint8_t>(val & 0x03);
        return b != 0 ? b : 1;
    }
    //@LABS-STUB
    static uint8_t normalizeBank(uint8_t val) {
        // TODO(1): mask to two bits, then wrap 0 -> 1
        return static_cast<uint8_t>(val & 0x03);
    }
    //@LABS-END

    uint8_t romByte(size_t off) const {
        // NUL shadow: reads past the image sample the low-idling bus.
        return off < rom_->size() ? (*rom_)[off] : kNullBus;
    }

    const std::vector<uint8_t>* rom_;
    std::vector<uint8_t> ram_ = std::vector<uint8_t>(0x1000, 0x00);      // A000-AFFF
    std::vector<uint8_t> scratch_ = std::vector<uint8_t>(0x0800, 0x00);  // E000-E7FF
    uint8_t bank_ = 1;  // reset value per spec
};

}  // namespace t8
