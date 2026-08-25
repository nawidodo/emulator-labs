#pragma once
#include <cstdint>

#include "mapper.hpp"

// Chapter 23 — MMC3 (mapper 4): the banked mapper with the scanline IRQ.
//
// Register decode (CPU writes to $8000-$FFFF; bit 0 picks odd/even role):
//   $8000-$9FFF even : bank select — command (bits 2-0) + CHR/PRG invert
//                      flags (bit 6 = CHR A12 inversion, bit 5 = PRG inversion)
//   $8000-$9FFF odd  : bank data for the latched command R0-R7
//   $A000-$BFFF even : mirroring — bit 0: 0 = VERTICAL, 1 = HORIZONTAL
//                      (inverted vs. the iNES flag!)
//   $C000-$DFFF even : IRQ latch (period-1; counter reloads from it)
//   $C000-$DFFF odd  : IRQ reload request (counter reloads on next edge)
//   $E000-$FFFF even : IRQ disable + acknowledge
//   $E000-$FFFF odd  : IRQ enable
//
// Bank commands:
//   R0/R1: 2 KiB CHR at $0000-$07FF / $0800-$0FFF (or $1000/$1800 when
//          CHR-inverted), R2-R5: 1 KiB CHR, R6: 8 KiB PRG at $8000 (or
//          $C000 when PRG-inverted), R7: 8 KiB PRG at $A000.
//   The other PRG half is fixed: second-to-last and last 8 KiB banks.
//
// IRQ model (course-simplified, documented in LECTURE.md):
//   The counter is clocked by PPU A12 rising edges (the harness calls
//   a12_edge() before each pattern-table fetch). On each edge:
//     1. if reload_pending            -> counter = latch, clear pending
//     2. else if counter == 0         -> counter = latch, assert IRQ if
//                                        enabled
//     3. else                         -> --counter
//   So a latch of L gives an interrupt every L+1 qualifying edges. The
//   assertion is level-held until an $E000 write acknowledges it.
namespace nes23mmc3 {

using nes23map::Cart;
using nes23map::Mapper;

//@LABS-BEGIN 1
//@LABS-SOLUTION
class Mmc3 : public Mapper {
public:
    explicit Mmc3(const Cart& c) : cart_(c) {}

    // ---- CPU side --------------------------------------------------------
    uint8_t cpu_read(uint16_t addr) override {
        if (addr < 0x8000) return 0;
        size_t bank = prg_bank_slot((addr >> 13) & 3);
        return cart_.prg[bank * 0x2000 + (addr & 0x1FFF)];
    }

    void cpu_write(uint16_t addr, uint8_t v) override {
        if (addr < 0x8000) return;
        switch ((addr >> 13) & 3) {  // $8000/$A000/$C000/$E000 -> 0/1/2/3
            case 1:  // $A000-$BFFF: mirroring (MMC3 polarity!)
                mirror_horizontal_ = (v & 1) != 0;
                break;
            case 2:  // $C000-$DFFF: IRQ latch / reload request
                if (addr & 1)
                    irq_reload_pending_ = true;
                else
                    irq_latch_ = v;          // latch ONLY — see 90_debug!
                break;
            case 3:  // $E000-$FFFF: IRQ disable+ack / enable
                if (addr & 1)
                    irq_enabled_ = true;
                else {
                    irq_enabled_ = false;
                    irq_pending_ = false;
                }
                break;
            default:                     // $8000-$9FFF: bank select/data
                if (addr & 1)
                    bank_data_[command_] = v;
                else {
                    command_ = v & 7;
                    chr_invert_ = (v & 0x40) != 0;
                    prg_invert_ = (v & 0x20) != 0;
                }
                break;
        }
    }

    // ---- PPU side --------------------------------------------------------
    uint8_t ppu_read(uint16_t addr) override {
        addr &= 0x1FFF;
        return cart_.chr[chr_offset(addr)];
    }

    void ppu_write(uint16_t addr, uint8_t v) override {
        if (!cart_.chr.empty()) return;
        // CHR-RAM MMC3 variants exist; keep flat writes as our fallback.
        flat_chr_[addr] = v;
    }

    // ---- IRQ machinery ---------------------------------------------------
    void a12_edge() override {
        if (irq_reload_pending_) {
            irq_counter_ = irq_latch_;
            irq_reload_pending_ = false;
        } else if (irq_counter_ == 0) {
            irq_counter_ = irq_latch_;
            if (irq_enabled_) irq_pending_ = true;
        } else {
            --irq_counter_;
        }
    }

    bool irq_line() const override { return irq_pending_; }

    int counter() const { return irq_counter_; }
    int latch() const { return irq_latch_; }
    bool enabled() const { return irq_enabled_; }
    bool mirroring_horizontal() const { return mirror_horizontal_; }

private:
    size_t prg_bank_slot(int window) const {
        const size_t banks = cart_.prg.size() / 0x2000;
        switch (window) {
            case 0:  // $8000: R6, or the fixed second-to-last when inverted
                return prg_invert_ ? banks - 2 : size_t(bank_data_[6]) % banks;
            case 1: return size_t(bank_data_[7]) % banks;   // always $A000
            case 2:  // $C000: fixed second-to-last, or R6 when inverted
                return prg_invert_ ? size_t(bank_data_[6]) % banks : banks - 2;
            default: return banks - 1;                      // fixed last
        }
    }

    size_t chr_offset(uint16_t addr) const {
        const size_t bytes = cart_.chr.empty() ? 0x2000 : cart_.chr.size();
        const size_t units1k = bytes / 0x400;
        addr &= 0x1FFF;
        int slice = int(addr >> 10);  // which 1 KiB slice of PPU CHR space
        bool high_half = (slice & 4) != 0;
        // Non-inverted: R0,R1 own the low 4 KiB; R2-R5 the high. Inverted
        // (bit 6 set): R2-R5 move down, R0/R1 move up.
        int r = !high_half ? (chr_invert_ ? 2 + (slice & 3) : slice / 2)
                           : (chr_invert_ ? int(slice - 4) : 2 + (slice & 3));
        size_t unit;
        switch (r) {  // R0/R1 are 2 KiB banks: two consecutive units
            case 0:
            case 1:
                unit = (size_t(bank_data_[r]) * 2 + size_t(slice & 1)) % units1k;
                break;
            default:
                unit = size_t(bank_data_[r]) % units1k;
                break;
        }
        return (unit * 0x400 + (addr & 0x3FF)) % bytes;
    }
    bool irq_pending_ = false;

    Cart cart_;
    std::vector<uint8_t> flat_chr_ = std::vector<uint8_t>(0x2000, 0x00);
    uint8_t bank_data_[8] = {0, 0, 2, 3, 4, 5, 6, 7};
    uint8_t command_ = 0;
    bool chr_invert_ = false;
    bool prg_invert_ = false;
    bool mirror_horizontal_ = false;
    int irq_latch_ = 0;
    int irq_counter_ = 0;
    bool irq_reload_pending_ = false;
    bool irq_enabled_ = false;
};
//@LABS-STUB
// TODO(1): MMC3 core — register decode above, R0-R7 banking with both
// invert modes, inverted-polarity mirroring register, and the documented
// three-step IRQ rule in a12_edge(). REMEMBER: $C000 stores the latch
// only; the counter learns about it through reloads.
class Mmc3 : public Mapper {
public:
    explicit Mmc3(const Cart& c) { (void)&c; }

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
    void a12_edge() override {}       // TODO(1)
    bool irq_line() const override { return false; }  // TODO(1)

    int counter() const { return 0; }             // TODO(1)
    int latch() const { return 0; }               // TODO(1)
    bool mirroring_horizontal() const { return false; }  // TODO(1)
};
//@LABS-END

}  // namespace nes23mmc3
