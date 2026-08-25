#pragma once
#include <cstdint>
#include <vector>

// Chapter 23 — the mapper interface every cartridge board implements.
// (Shared contract for all board exercises; see 01_ines_header.)
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

struct Cart {
    std::vector<uint8_t> prg;
    std::vector<uint8_t> chr;
};

}  // namespace nes23map
