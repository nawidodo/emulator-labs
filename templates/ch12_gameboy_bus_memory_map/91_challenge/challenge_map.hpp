// challenge_map.hpp — CourseBoy-II map under repair.
//
// The interface matches the exercise-01 Bus shape on purpose: once you
// have implemented device routing, an unseen map is a spec-reading
// exercise, not a new architecture. The full hardware spec lives in
// CHALLENGE.md next to this file — it is authoritative.
//
// Six TODO blocks, one per window rule. The top-level dispatch in
// read()/write() is provided complete; your job is each region's rule.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cb2 {

// CourseBoy-II ties its data strobes HIGH on empty slots: open bus $FF.
constexpr uint8_t kOpenBus = 0xFF;

constexpr uint16_t kMirrorDelta = 0x4000;  // 4000-7FFF mirrors 0000-3FFF
constexpr uint16_t kAliasBase = 0xE000;    // E000-EFFF alias C000-CFFF
constexpr uint16_t kVramPage = 0x0800;     // window size = one page

class Map {
public:
    virtual ~Map() = default;
    virtual uint8_t read(uint16_t addr) = 0;
    virtual void write(uint16_t addr, uint8_t val) = 0;
};

class Cb2Map final : public Map {
public:
    explicit Cb2Map(const std::vector<uint8_t>& rom) : rom_(&rom) {}

    // ---- test seams --------------------------------------------------
    uint8_t vramPage() const { return pageBit_; }
    uint8_t peekVram(size_t absOff) const { return vram_[absOff]; }  // 0x000-0xFFF
    uint8_t peekRam(size_t off) const { return ram_[off]; }          // 0x000-0xFFF

    // ---- provided dispatch: which region, then the region's rule ----
    uint8_t read(uint16_t addr) override {
        if (addr <= 0x7FFF) return readRom(addr);
        if (addr >= 0x8000 && addr <= 0x87FF)
            return readVram(static_cast<uint16_t>(addr - 0x8000));
        if ((addr >= 0xC000 && addr <= 0xCFFF) ||
            (addr >= kAliasBase && addr <= 0xEFFF))
            return readWorkRam(addr);
        if (addr >= 0xFF00 && addr <= 0xFF0F) return readSys(addr);
        return kOpenBus;
    }

    void write(uint16_t addr, uint8_t val) override {
        if (addr <= 0x7FFF) return;  // ROM half: writes ignored
        if (addr >= 0x8000 && addr <= 0x87FF)
            writeVram(static_cast<uint16_t>(addr - 0x8000), val);
        else if ((addr >= 0xC000 && addr <= 0xCFFF) ||
                 (addr >= kAliasBase && addr <= 0xEFFF))
            writeWorkRam(addr, val);
        else if (addr >= 0xFF00 && addr <= 0xFF0F)
            writeSys(addr, val);
        // everything else: open bus, dropped
    }

private:
    //@LABS-BEGIN 1
    //@LABS-SOLUTION
    // 0000-3FFF fixed bank 0; 4000-7FFF mirrors it exactly.
    uint8_t readRom(uint16_t addr) const {
        return romByte(addr < kMirrorDelta ? addr
                                           : static_cast<uint16_t>(addr - kMirrorDelta));
    }
    //@LABS-STUB
    uint8_t readRom(uint16_t addr) const {
        // TODO(1): 4000-7FFF must MIRROR 0000-3FFF (see CHALLENGE.md)
        return romByte(addr);
    }
    //@LABS-END

    //@LABS-BEGIN 2
    //@LABS-SOLUTION
    // The CPU never sees both pages at once: only the selected one is
    // visible through the 2 KiB window.
    uint8_t readVram(uint16_t off) const {
        return vram_[static_cast<size_t>(pageBit_) * kVramPage + off];
    }
    //@LABS-STUB
    uint8_t readVram(uint16_t off) const {
        // TODO(2): route through the page selected by SYS bit 0
        return vram_[off];
    }
    //@LABS-END

    //@LABS-BEGIN 3
    //@LABS-SOLUTION
    // C000-CFFF direct; E000-EFFF same cells via -$2000 (E000 sits on C000).
    uint8_t readWorkRam(uint16_t addr) const {
        return ram_[addr <= 0xDFFF ? addr - 0xC000 : addr - kAliasBase];
    }
    //@LABS-STUB
    uint8_t readWorkRam(uint16_t addr) const {
        // TODO(3): E000-EFFF must alias C000-CFFF with translation -$2000
        return addr <= 0xDFFF ? ram_[addr - 0xC000] : kOpenBus;
    }
    //@LABS-END

    //@LABS-BEGIN 4
    //@LABS-SOLUTION
    void writeSys(uint16_t addr, uint8_t val) {
        if (addr == 0xFF00) pageBit_ = static_cast<uint8_t>(val & 0x01);  // reserved bits ignored
    }
    //@LABS-STUB
    void writeSys(uint16_t addr, uint8_t val) {
        // TODO(4): FF00 keeps ONLY bit 0 of val; other SYS regs are dead
        if (addr == 0xFF00) pageBit_ = val;
    }
    //@LABS-END

    //@LABS-BEGIN 5
    //@LABS-SOLUTION
    void writeVram(uint16_t off, uint8_t val) {
        vram_[static_cast<size_t>(pageBit_) * kVramPage + off] = val;
    }
    //@LABS-STUB
    void writeVram(uint16_t off, uint8_t val) {
        // TODO(5): writes must land in the SELECTED page, not always page 0
        vram_[off] = val;
    }
    //@LABS-END

    //@LABS-BEGIN 6
    //@LABS-SOLUTION
    void writeWorkRam(uint16_t addr, uint8_t val) {
        if (addr <= 0xDFFF) ram_[addr - 0xC000] = val;
        else ram_[addr - kAliasBase] = val;
    }
    //@LABS-STUB
    void writeWorkRam(uint16_t addr, uint8_t val) {
        // TODO(6): alias-side writes (E000-EFFF) must reach C000-CFFF too
        if (addr <= 0xDFFF) ram_[addr - 0xC000] = val;
    }
    //@LABS-END

    // Provided complete: SYS readback + short-image ROM padding.
    uint8_t readSys(uint16_t addr) const {
        return addr == 0xFF00 ? pageBit_ : 0x00;  // only FF00 is implemented
    }
    uint8_t romByte(size_t off) const {
        return off < rom_->size() ? (*rom_)[off] : kOpenBus;  // short images pad $FF
    }

    const std::vector<uint8_t>* rom_;
    std::vector<uint8_t> vram_ = std::vector<uint8_t>(2 * kVramPage, 0x00);
    std::vector<uint8_t> ram_ = std::vector<uint8_t>(0x1000, 0x00);
    uint8_t pageBit_ = 0;
};

}  // namespace cb2
