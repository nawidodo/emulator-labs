#pragma once
#include <cstdint>

#include "mapper.hpp"

// Chapter 23 — the two simple switchable boards.
//
// UxROM (mapper 2): 16 KiB PRG banks. The CPU window $8000-$BFFF shows the
// bank selected by ANY write to $8000-$FFFF; $C000-$FFFF is hard-wired to
// the LAST bank (that is why reset code always lives there). CHR is RAM:
// the PPU sees a flat, writable 8 KiB.
//
// CNROM (mapper 3): PRG never switches ($8000-$BFFF = bank 0, $C000-$FFFF
// = last bank, mirrored when only 16 KiB exists). CHR ROM banks of 8 KiB
// are selected by any CPU write; the low bits are masked to what fits.
namespace nes23uxcn {

using nes23map::Cart;
using nes23map::Mapper;

constexpr uint16_t kBankSize = 0x4000;  // 16 KiB PRG units

inline size_t prg_banks(const Cart& c) {
    return (c.prg.size() + kBankSize - 1) / kBankSize;
}

//@LABS-BEGIN 1
//@LABS-SOLUTION
class UxRom : public Mapper {
public:
    explicit UxRom(const Cart& c) : cart_(c) {}  // copy: outlives temps

    uint8_t cpu_read(uint16_t addr) override {
        if (addr < 0x8000) return 0;  // no PRG RAM on this board
        size_t bank =
            addr < 0xC000 ? size_t(prg_bank_) : prg_banks(cart_) - 1;
        return cart_.prg[bank * kBankSize + (addr & 0x3FFF)];
    }

    // Any write into the PRG window re-latches the switchable bank.
    void cpu_write(uint16_t addr, uint8_t v) override {
        if (addr >= 0x8000)
            prg_bank_ = v & uint8_t(prg_banks(cart_) - 1);
    }

    uint8_t ppu_read(uint16_t addr) override {
        return chr_ram_[addr & 0x1FFF];
    }
    void ppu_write(uint16_t addr, uint8_t v) override {
        chr_ram_[addr & 0x1FFF] = v;
    }

    uint8_t bank() const { return prg_bank_; }

private:
    Cart cart_;
    uint8_t prg_bank_ = 0;
    std::vector<uint8_t> chr_ram_ = std::vector<uint8_t>(0x2000, 0x00);
};
//@LABS-STUB
// TODO(1): UxROM — switchable low half via any $8000-$FFFF write (mask the
// bank value to installed banks), fixed LAST bank in the high half,
// writable flat 8 KiB CHR RAM.
class UxRom : public Mapper {
public:
    explicit UxRom(const Cart& c) { (void)&c; }

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
    uint8_t bank() const { return 0; }  // TODO(1)
};
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
class CnRom : public Mapper {
public:
    explicit CnRom(const Cart& c) : cart_(c) {}  // copy: outlives temps

    uint8_t cpu_read(uint16_t addr) override {
        if (addr < 0x8000) return 0;
        size_t banks = prg_banks(cart_);
        size_t bank = addr < 0xC000 ? size_t(0) : banks - 1;
        return cart_.prg[bank * kBankSize + (addr & 0x3FFF)];
    }

    // Any PRG-window write selects an 8 KiB CHR bank (masked to fit).
    void cpu_write(uint16_t /*addr*/, uint8_t v) override {
        size_t units = cart_.chr.empty() ? 1 : cart_.chr.size() / 0x2000;
        chr_bank_ = size_t(v) & (units - 1);
    }

    uint8_t ppu_read(uint16_t addr) override {
        if (cart_.chr.empty()) return 0;
        return cart_.chr[chr_bank_ * 0x2000 + (addr & 0x1FFF)];
    }
    void ppu_write(uint16_t /*addr*/, uint8_t /*v*/) override {
        // CNROM has CHR ROM: PPU writes are discarded.
    }

    size_t bank() const { return chr_bank_; }

private:
    Cart cart_;
    size_t chr_bank_ = 0;
};
//@LABS-STUB
// TODO(2): CNROM — fixed PRG (bank 0 low, last bank high), CHR bank latch
// on ANY CPU write masked to installed 8 KiB units, PPU writes dropped.
class CnRom : public Mapper {
public:
    explicit CnRom(const Cart& c) { (void)&c; }

    uint8_t cpu_read(uint16_t addr) override {
        (void)addr;
        return 0;  // TODO(2)
    }
    void cpu_write(uint16_t addr, uint8_t v) override {
        (void)addr; (void)v;  // TODO(2)
    }
    uint8_t ppu_read(uint16_t addr) override {
        (void)addr;
        return 0;  // TODO(2)
    }
    void ppu_write(uint16_t addr, uint8_t v) override {
        (void)addr; (void)v;  // TODO(2)
    }
    size_t bank() const { return 0; }  // TODO(2)
};
//@LABS-END

}  // namespace nes23uxcn
