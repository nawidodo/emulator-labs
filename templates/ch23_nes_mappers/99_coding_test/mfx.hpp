#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

// Chapter 23 coding test — the MFX-1, a fictional mapper specified in
// CODING_TEST.md. Implement exactly what the spec says; the grader runs
// unseen op scripts against your implementation and hashes the log.
//
//   R0-R3 live at $8000-$BFFF; the write address bits 1-0 pick the reg.
//   Writes to $C000-$FFFF are ignored. Unmapped CPU reads return 0x00.
namespace nes23mfx {

using nes23cart::Cart;

//@LABS-BEGIN 1
//@LABS-SOLUTION
class Mfx1 {
public:
    explicit Mfx1(const Cart& c) : cart_(c) {}

    uint8_t cpu_read(uint16_t addr) {
        if (addr >= 0x6000 && addr < 0x8000)
            return prg_ram_[addr & 0x1FFF];
        if (addr >= 0x8000 && addr < 0xC000) {
            size_t off = size_t(r_[0] & prg_mask()) * 0x4000 +
                         (addr & 0x3FFF);
            return cart_.prg[off % cart_.prg.size()];
        }
        if (addr >= 0xC000) {
            // Mode A: fixed last bank. Mode B (R1 bit 0): mirror R0.
            size_t bank = (r_[1] & 0x01) ? size_t(r_[0] & prg_mask())
                                         : cart_.prg.size() / 0x4000 - 1;
            return cart_.prg[(bank * 0x4000 + (addr & 0x3FFF)) %
                             cart_.prg.size()];
        }
        return 0;
    }

    void cpu_write(uint16_t addr, uint8_t v) {
        if (addr >= 0x6000 && addr < 0x8000) {
            prg_ram_[addr & 0x1FFF] = v;
            return;
        }
        if (addr < 0x8000 || addr >= 0xC000) return;
        ++writes_;
        int reg = addr & 3;
        r_[reg] = v;
        if (reg == 3) {           // re-arming the timer also acknowledges
            timer_armed_ = true;
            timer_count_ = v & 0x1F;
            irq_ = false;
        } else if (timer_armed_ && (writes_ & 3) == 0) {
            if (--timer_count_ == 0) {
                timer_armed_ = false;
                irq_ = true;
            }
        }
    }

    uint8_t ppu_read(uint16_t addr) {
        addr &= 0x1FFF;
        if (chr_units() == 0) return chr_ram_[addr];
        // Echo CHR: one 2 KiB unit replicated across all four quadrants.
        return cart_.chr[(size_t(r_[2] & uint8_t(chr_units() - 1)) * 0x800 +
                          (addr & 0x07FF)) %
                         cart_.chr.size()];
    }
    std::string debug_snapshot() const {
        char b[80];
        std::snprintf(b, sizeof(b),
                      "mfx r0=%02x r1=%02x r2=%02x r3=%02x wc=%u irq=%d",
                      r_[0], r_[1], r_[2], r_[3], unsigned(writes_),
                      int(irq_));
        return b;
    }

    void ppu_write(uint16_t addr, uint8_t v) {
        if (chr_units() != 0) return;  // CHR ROM: writes dropped
        chr_ram_[addr & 0x1FFF] = v;
    }

    bool irq_line() const { return irq_; }
    int timer_count() const { return timer_count_; }
    uint32_t writes() const { return writes_; }

private:
    size_t prg_mask() const { return cart_.prg.size() / 0x4000 - 1; }
    size_t chr_units() const { return cart_.chr.size() / 0x800; }

    Cart cart_;
    std::vector<uint8_t> prg_ram_ = std::vector<uint8_t>(0x2000, 0x00);
    std::vector<uint8_t> chr_ram_ = std::vector<uint8_t>(0x2000, 0x00);
    uint8_t r_[4] = {0, 0, 0, 0};
    uint32_t writes_ = 0;
    bool timer_armed_ = false;
    int timer_count_ = 0;
    bool irq_ = false;
};
//@LABS-STUB
// TODO(1): register file at $8000-$BFFF selected by addr bits 1-0,
// $C000-$FFFF writes ignored, 8 KiB PRG RAM at $6000-$7FFF.
// TODO(2): PRG mapping — $8000-$BFFF from R0 (masked); $C000-$FFFF fixed
// last bank, or mirrors R0 when R1 bit 0 is set. Unmapped reads -> 0x00.
// TODO(3): echo CHR — the single 2 KiB unit (R2 masked) replicated over
// the whole $0000-$1FFF window; flat CHR RAM when the cart has no CHR ROM.
// TODO(4): one-shot IRQ timer in R3 (period = value bits 4-0). It ticks
// once per FOURTH register write (global counter), latches irq on zero,
// and a fresh R3 write rearms + acknowledges.
class Mfx1 {
public:
    explicit Mfx1(const Cart& c) { (void)&c; }

    uint8_t cpu_read(uint16_t addr) {
        (void)addr;
        return 0;  // TODO(2)
    }
    void cpu_write(uint16_t addr, uint8_t v) {
        (void)addr; (void)v;  // TODO(1)
    }
    uint8_t ppu_read(uint16_t addr) {
        (void)addr;
        return 0;  // TODO(3)
    }
    void ppu_write(uint16_t addr, uint8_t v) {
        (void)addr; (void)v;  // TODO(3)
    }
    std::string debug_snapshot() const { return "mfx TODO(1)"; }
    bool irq_line() const { return false; }        // TODO(4)
    int timer_count() const { return 0; }          // TODO(4)
    uint32_t writes() const { return 0; }          // TODO(4)

private:
    Cart cart_;
};
//@LABS-END

}  // namespace nes23mfx
