#pragma once
#include <cstdint>

#include "mapper.hpp"

// Chapter 23 — MMC1 (mapper 1): the serial shift-register mapper.
//
// MMC1 has no direct bank registers. Every CPU write to $8000-$FFFF feeds
// ONE bit (data bit 0) into a 5-bit shift register; on the fifth write the
// accumulated value is dispatched to one of four internal registers
// selected by its top two bits. A write with bit 7 set resets the shift
// register — games "flush" it that way before loading.
//
// Dispatched register (chosen by WHICH ADDRESS WINDOW received the fifth
// write):
//   $8000-$9FFF: Control — CHR mode (bit4), PRG mode (bits3-2),
//                mirroring (bits1-0)
//   $A000-$BFFF: CHR bank 0 ($0000-$0FFF in 4 KiB mode)
//   $C000-$DFFF: CHR bank 1 ($1000-$1FFF in 4 KiB mode)
//   $E000-$FFFF: PRG bank (meaning depends on PRG mode)
//
// PRG modes (control bits 3-2):
//   0,1 : 32 KiB switchable at $8000 (bank bit 0 ignored)
//   2   : fixed first bank at $8000, switchable $C000-$FFFF
//   3   : switchable $8000-$BFFF, fixed LAST bank at $C000-$FFFF
//
// Mirroring (control bits 1-0): 0=one-screen lower, 1=one-screen upper,
// 2=vertical, 3=horizontal. Note the order — vertical is 2 here, NOT 1.
//
// Writing to the shift register with bit 7 set ALSO forces PRG mode bits to
// 'fix last' (hardware ORs 0x0C into the control register).
namespace nes23mmc1 {

using nes23map::Cart;
using nes23map::Mapper;

enum class Mirroring : uint8_t {
    OneScreenLower = 0,
    OneScreenUpper = 1,
    Vertical = 2,
    Horizontal = 3,
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
class Mmc1 : public Mapper {
public:
    explicit Mmc1(const Cart& c) : cart_(c) {}  // copy: outlives temps

    // --- CPU side ---------------------------------------------------------
    uint8_t cpu_read(uint16_t addr) override {
        if (addr < 0x6000) return 0;
        if (addr < 0x8000) return prg_ram_[addr & 0x1FFF];
        size_t off = prg_offset(addr);
        return cart_.prg[off % cart_.prg.size()];
    }

    void cpu_write(uint16_t addr, uint8_t v) override {
        if (addr >= 0x6000 && addr < 0x8000) {
            prg_ram_[addr & 0x1FFF] = v;
            return;
        }
        if (addr < 0x8000) return;
        if (v & 0x80) {  // MSB set: reset the shift register...
            shift_ = 0;
            bits_ = 0;
            control_ |= 0x0C;  // ...and force PRG mode to "fix last"
        } else {
            shift_ = uint8_t((shift_ >> 1) | ((v & 1) << 4));
            if (++bits_ == 5) {
                dispatch(addr, shift_);
                shift_ = 0;
                bits_ = 0;
            }
        }
    }

    // --- PPU side ---------------------------------------------------------
    uint8_t ppu_read(uint16_t addr) override {
        addr &= 0x1FFF;
        if (cart_.chr.empty()) return chr_ram_[addr];  // CHR-RAM board
        size_t unit = addr < 0x1000 ? chr_unit_low() : chr_unit_high();
        return cart_.chr[(unit * 0x1000 + (addr & 0x0FFF)) %
                         cart_.chr.size()];
    }

    void ppu_write(uint16_t addr, uint8_t v) override {
        if (!cart_.chr.empty()) return;  // CHR ROM is read-only
        chr_ram_[addr & 0x1FFF] = v;
    }

    // --- introspection for tests ------------------------------------------
    uint8_t control() const { return control_; }
    uint8_t latch_value() const { return shift_; }  // bits accumulated so far
    int pending_bits() const { return bits_; }
    Mirroring mirroring() const {
        return Mirroring(control_ & 0x03);
    }

private:
    // The written ADDRESS WINDOW picks the register; the 5 accumulated
    // bits are pure payload.
    void dispatch(uint16_t addr, uint8_t value) {
        switch ((addr >> 13) & 3) {  // $8000/$A000/$C000/$E000 -> 0/1/2/3
            case 0: control_ = value & 0x1F; break;
            case 1: chr0_ = value & 0x1F; break;
            case 2: chr1_ = value & 0x1F; break;
            default: prg_ = value & 0x1F; break;
        }
    }

    size_t last_bank() const { return cart_.prg.size() / 0x4000 - 1; }

    size_t prg_offset(uint16_t addr) const {
        const size_t kBank = 0x4000;
        switch ((control_ >> 2) & 3) {
            case 2:  // fix FIRST at $8000, switch $C000
                if (addr < 0xC000) return 0 * kBank + (addr & 0x3FFF);
                return size_t(prg_ & 0x0F) * kBank + (addr & 0x3FFF);
            case 3:  // switch $8000, fix LAST at $C000
                if (addr < 0xC000)
                    return size_t(prg_ & 0x0F) * kBank + (addr & 0x3FFF);
                return last_bank() * kBank + (addr & 0x3FFF);
            default: {  // 32 KiB window across both halves
                size_t base = size_t(prg_ & 0x0E);
                size_t half = (addr < 0xC000) ? 0 : 1;
                return (base + half) * kBank + (addr & 0x3FFF);
            }
        }
    }

    size_t units4k() const { return cart_.chr.empty() ? 8 : cart_.chr.size() / 0x1000; }

    size_t chr_unit_low() const {
        if ((control_ & 0x10) == 0)  // 8 KiB mode: low half = even unit of chr0
            return (size_t(chr0_) & ~size_t(1)) % units4k();
        return size_t(chr0_) % units4k();
    }

    size_t chr_unit_high() const {
        if ((control_ & 0x10) == 0)  // 8 KiB mode: high half follows
            return (size_t(chr0_) | size_t(1)) % units4k();
        return size_t(chr1_) % units4k();
    }

    Cart cart_;
    std::vector<uint8_t> prg_ram_ = std::vector<uint8_t>(0x2000, 0x00);
    std::vector<uint8_t> chr_ram_ = std::vector<uint8_t>(0x2000, 0x00);
    uint8_t shift_ = 0;    // accumulating shift register
    int bits_ = 0;         // writes since last reset/dispatch
    uint8_t control_ = 0x0C;  // power-on: PRG mode 3 (fix last), mirr. lower
    uint8_t chr0_ = 0, chr1_ = 0, prg_ = 0;
};
//@LABS-STUB
// TODO(1): MMC1 core. Feed data bit 0 into a 5-bit shift register per CPU
// write to $8000-$FFFF; bit-7 writes reset it and OR $0C into control.
// On the fifth write, dispatch by value bits 14-13 into control/chr0/chr1/
// prg. Implement banking exactly as documented above plus 8 KiB PRG RAM at
// $6000-$7FFF.
class Mmc1 : public Mapper {
public:
    explicit Mmc1(const Cart& c) { (void)&c; }

    uint8_t cpu_read(uint16_t addr) override {
        (void)addr;
        return 0;  // TODO(1)
    }
    void cpu_write(uint16_t addr, uint8_t v) override {
        (void)addr; (void)v;  // TODO(1)
    }
    uint8_t ppu_read(uint16_t addr) override {
        (void)addr;
        return 0;  // TODO(1)
    }
    void ppu_write(uint16_t addr, uint8_t v) override {
        (void)addr; (void)v;  // TODO(1)
    }
    uint8_t control() const { return 0; }       // TODO(1)
    uint8_t latch_value() const { return 0; }   // TODO(1)
    int pending_bits() const { return 0; }      // TODO(1)
    Mirroring mirroring() const { return Mirroring::Horizontal; }  // TODO(1)

private:
    Cart cart_;
};
//@LABS-END

}  // namespace nes23mmc1
