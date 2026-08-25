#pragma once
#include <cstdint>
#include <vector>

// Chapter 23 — the mapper interface every cartridge board implements.
//
// The CPU and PPU never see bank registers; they see memory. A mapper is
// therefore just four address-space functions plus two hardware lines:
//
//   cpu_read / cpu_write   the $6000-$FFFF window (PRG, PRG-RAM, regs)
//   ppu_read / ppu_write   the $0000-$1FFF window (CHR ROM/RAM)
//   irq_line()             level-sensitive IRQ line into the CPU
//   a12_edge()             notification hook: PPU A12 had a rising edge
//                          (only MMC3-class mappers care)
//
// Harness tests in this chapter call these functions DIRECTLY — no CPU
// core needed to pin banking behavior.
namespace nes23map {

class Mapper {
public:
    virtual ~Mapper() = default;

    virtual uint8_t cpu_read(uint16_t addr) = 0;
    virtual void cpu_write(uint16_t addr, uint8_t v) = 0;
    virtual uint8_t ppu_read(uint16_t addr) = 0;
    virtual void ppu_write(uint16_t addr, uint8_t v) = 0;

    virtual bool irq_line() const { return false; }
    virtual void a12_edge() {}

protected:
    Mapper() = default;
};

// Minimal cart payload shared by every board below. PRG is a whole number
// of 16 KiB banks; CHR is 8 KiB units (possibly zero bytes: CHR RAM).
struct Cart {
    std::vector<uint8_t> prg;
    std::vector<uint8_t> chr;
};

}  // namespace nes23map
