#pragma once
#include <array>
#include <cstdint>

// Chapter 21 — the PPU has its OWN address bus, separate from CPU memory.
// Curriculum §57: every device must be instantiable and testable alone.
//
//   $0000-$1FFF  pattern tables (CHR ROM/RAM, usually cartridge-side)
//   $2000-$2EFF  nametables (2 KB physical VRAM, mirrored per cartridge)
//   $3000-$3EFF  mirror of $2000-$2EFF (never used by real games)
//   $3F00-$3F1F  palette RAM (internally mirrored through $3FFF)
//
namespace nes21bus {

// Mirroring describes how the PPU's 2 KB of nametable RAM is wired to the
// four logical nametable windows. Cartridges choose the wiring.
//   Horizontal ("vertical arrangement", iNES flags6 bit0 = 0):  A A / B B
//       -> $2000 == $2400, $2800 == $2C00   (address bit 11 selects)
//   Vertical   ("horizontal arrangement", flags6 bit0 = 1):     A B / A B
//       -> $2000 == $2800, $2400 == $2C00   (address bit 10 selects)
//   FourScreen (rare): 4 KB of dedicated nametable RAM, no mirroring.
enum class Mirroring : uint8_t { Horizontal = 0, Vertical = 1, FourScreen = 2 };

class PpuBus {
public:
    // Backing stores are public so fixtures/tests can craft states directly.
    std::array<uint8_t, 0x2000> chr{};       // pattern tables $0000-$1FFF
    std::array<uint8_t, 0x1000> vram{};      // nametable RAM (2 KB, 4 KB if FourScreen)
    std::array<uint8_t, 0x0020> palette{};   // $3F00-$3F1F

    explicit PpuBus(Mirroring m = Mirroring::Horizontal) : mirroring_(m) {}

    void set_mirroring(Mirroring m) { mirroring_ = m; }
    Mirroring mirroring() const { return mirroring_; }

    // Map any nametable-window address ($2000-$3EFF) onto physical VRAM.
    // The $3000-$3EFF range folds down onto $2000-$2EFF before mirroring.
    //
    //@LABS-BEGIN 1
    //@LABS-SOLUTION
    uint16_t nametable_index(uint16_t addr) const {
        addr &= 0x0FFF;  // fold $3000-$3EFF onto $2000-$2EFF
        uint16_t page;
        switch (mirroring_) {
            case Mirroring::Vertical:    // A B / A B  -> bit 10 selects
                page = (addr >> 10) & 1;
                break;
            case Mirroring::FourScreen:  // four independent 1 KB pages
                return addr;
            case Mirroring::Horizontal:  // A A / B B  -> bit 11 selects
            default:
                page = (addr >> 11) & 1;
                break;
        }
        return uint16_t((page << 10) | (addr & 0x03FF));
    }
    //@LABS-STUB
    // TODO(1): map a nametable address ($2000-$3EFF) to physical VRAM.
    // First fold $3000-$3EFF onto $2000-$2EFF (addr &= 0x0FFF), then apply
    // the cartridge mirroring: Vertical selects the page with address
    // bit 10, Horizontal with bit 11, FourScreen uses addr directly.
    // The stub below ignores mirroring, so mirror tests run RED.
    uint16_t nametable_index(uint16_t addr) const {
        (void)mirroring_;
        return uint16_t(addr & 0x03FF);  // wrong: no folding, no mirroring
    }
    //@LABS-END

    // Map any palette address ($3F00-$3FFF) into the 32-entry palette RAM.
    // Hardware detail worth getting exactly right: writes to the "sprite
    // palette entry 0" slots ($3F10/$3F14/$3F18/$3F1C) land on the
    // corresponding backdrop entries ($3F00/$3F04/$3F08/$3F0C) because
    // entry 0 of each sprite palette is never displayed (transparent).
    //
    //@LABS-BEGIN 2
    //@LABS-SOLUTION
    static uint16_t palette_index(uint16_t addr) {
        addr &= 0x1F;  // $3F20-$3FFF mirrors $3F00-$3F1F
        if ((addr & 0x13) == 0x10) addr &= ~0x10;
        return addr;
    }
    //@LABS-STUB
    // TODO(2): fold $3F00-$3FFF onto 0-31 and redirect $3F10/14/18/1C to
    // $3F00/04/08/0C. Hint: (addr & 0x13) == 0x10 identifies exactly the
    // four mirrored slots. The stub skips the redirection on purpose.
    static uint16_t palette_index(uint16_t addr) {
        return uint16_t(addr & 0x1F);  // wrong: backdrop mirroring missing
    }
    //@LABS-END

    //@LABS-BEGIN 3
    //@LABS-SOLUTION
    uint8_t read(uint16_t addr) const {
        if (addr <= 0x1FFF) return chr[addr];
        if (addr < 0x3F00) return vram[nametable_index(addr)];
        return palette[palette_index(addr)];
    }

    void write(uint16_t addr, uint8_t value) {
        if (addr <= 0x1FFF) {
            chr[addr] = value;
        } else if (addr < 0x3F00) {
            vram[nametable_index(addr)] = value;
        } else {
            palette[palette_index(addr)] = value;
        }
    }
    //@LABS-STUB
    // TODO(3): dispatch reads/writes across the three regions:
    //   $0000-$1FFF -> chr, $2000-$3EFF -> vram[nametable_index(addr)],
    //   $3F00-$3FFF -> palette[palette_index(addr)].
    // Stubs return 0 / drop writes so the suite runs RED until finished.
    uint8_t read(uint16_t /*addr*/) const {
        return 0;  // wrong on purpose
    }

    void write(uint16_t /*addr*/, uint8_t /*value*/) {
        // wrong on purpose: writes dropped
    }
    //@LABS-END

private:
    Mirroring mirroring_;
};

}  // namespace nes21bus
