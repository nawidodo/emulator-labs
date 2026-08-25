// nesbus.hpp — NES CPU bus, exercise 01: RAM/PPU/APU/controller decode.
//
// The 6502 sees only addresses; the bus decides what answers. This chapter
// wires the internal regions — mirrored 2KB RAM, mirrored PPU registers,
// APU/IO space, controller shift registers, OAM DMA — and leaves the
// cartridge socket ($6000-$FFFF) for exercise 02.
//
// Region map (CPU $0000-$FFFF):
//   $0000-$1FFF  2KB internal RAM, mirrored every $0800
//   $2000-$3FFF  PPU registers, mirrored every 8 bytes
//   $4000-$4013  APU (stubbed: writes swallowed)
//   $4014        OAM DMA trigger
//   $4015        APU status (reads 0)
//   $4016-$4017  controllers 1/2 (strobed shift registers)
//   $4018-$401F  IO (open bus stand-in)
//   $6000-$FFFF  cartridge (via Mapper*, absent here -> open bus)
#pragma once

#include <array>
#include <cstdint>

namespace nesbus {

// ---- controller: strobe-latched shift register --------------------------

class Controller {
public:
    // Read order after a strobe: A first, then B, Select, Start,
    // Up, Down, Left, Right — so pack them LSB-first.
    enum Buttons : uint8_t {
        BtnA = 0x01, BtnB = 0x02, Select = 0x04, Start = 0x08,
        Up = 0x10, Down = 0x20, Left = 0x40, Right = 0x80,
    };

    void set_buttons(uint8_t pressed) { buttons_ = pressed; }

//@LABS-BEGIN 3
//@LABS-SOLUTION
    // While the strobe is HIGH the shift register keeps reloading with the
    // current button state, so reads report the A button over and over.
    // The 1->0 transition latches the snapshot; subsequent reads walk the
    // eight bits and then report 1s (open-bus convention on real pads).
    void write_strobe(uint8_t val) {
        const bool high = (val & 0x01) != 0;
        if (high) shifter_ = buttons_;
        strobe_ = high;
    }

    uint8_t read() {
        if (strobe_) shifter_ = buttons_;
        const uint8_t bit = shifter_ & 0x01;
        if (!strobe_) shifter_ = static_cast<uint8_t>((shifter_ >> 1) | 0x80);
        // Bits 5-7 of $4016 float high on a stock NES.
        return static_cast<uint8_t>(bit | 0x40);
    }
//@LABS-STUB
    // TODO(3): strobe semantics. While val&1 is HIGH every read reports the
    // CURRENT state of button A and nothing shifts. On the falling edge the
    // eight button bits are latched; later reads shift one bit out per read
    // (LSB first: A .. Right) and feed 1s in from the top. Bit 6 of the
    // returned byte reads as 1 (open bus).
    void write_strobe(uint8_t) {}

    uint8_t read() { return 0x40; }  // TODO(3)
//@LABS-END

private:
    uint8_t buttons_ = 0;
    uint8_t shifter_ = 0;
    bool strobe_ = false;
};

// ---- PPU register file stub (the real PPU arrives in ch21) -------------

struct PpuRegs {
    std::array<uint8_t, 8> r{};
};

// ---- the bus ------------------------------------------------------------

class NesBus {
public:
    PpuRegs ppu;
    Controller controller[2];
    // Primary OAM: 256 bytes copied in by $4014 DMA.
    std::array<uint8_t, 256> oam{};
    // CPU cycle counter; OAM DMA debits against it (see trigger_oam_dma).
    uint64_t cycles = 0;

//@LABS-BEGIN 1
//@LABS-SOLUTION
    // $0000-$1FFF is the internal 2KB RAM repeated eight times: only A10-A0
    // reach the chip, every other address line is ignored.
    uint8_t ram_read(uint16_t addr) const {
        return ram_[addr & 0x07FF];
    }

    void ram_write(uint16_t addr, uint8_t v) {
        ram_[addr & 0x07FF] = v;
    }
//@LABS-STUB
    // TODO(1): the RAM window covers $0000-$1FFF but the chip only sees
    // 11 address lines — $0800, $1000, $1800 ... all land in the same 2KB.
    // Mask the address accordingly for reads AND writes.
    uint8_t ram_read(uint16_t addr) const {
        return ram_[addr & 0x00FF];  // TODO(1)
    }

    void ram_write(uint16_t addr, uint8_t v) {
        ram_[addr & 0x00FF] = v;     // TODO(1)
    }
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
    // $2000-$3FFF: eight registers, mirrored every 8 bytes. Only A2-A0 are
    // connected; $2008 IS $2000, and so is $3008 or $3FF8.
    uint8_t ppu_read(uint16_t addr) {
        return ppu.r[addr & 0x0007];
    }

    void ppu_write(uint16_t addr, uint8_t v) {
        ppu.r[addr & 0x0007] = v;
    }
//@LABS-STUB
    // TODO(2): decode the PPU window down to its eight registers. This
    // version keeps only A1-A0, so half the register file aliases wrongly.
    uint8_t ppu_read(uint16_t addr) {
        return ppu.r[addr & 0x0003];  // TODO(2)
    }

    void ppu_write(uint16_t addr, uint8_t v) {
        ppu.r[addr & 0x0003] = v;     // TODO(2)
    }
//@LABS-END

    uint8_t controller_read(uint16_t addr) {
        return controller[addr & 0x0001].read();
    }

//@LABS-BEGIN 4
//@LABS-SOLUTION
    // Writing $4014 latches a page number; the DMA unit copies 256 bytes
    // of that CPU page into primary OAM. The whole operation is an
    // authoritative debit: exactly 513 cycles (1 dummy + 256 read/write
    // pairs), PLUS one more when the operation starts on an odd CPU cycle
    // (the get/put alignment quirk). The reads below bill through read()
    // for the bus transcript, then we pin the counter to the official total.
    void trigger_oam_dma(uint8_t page) {
        const uint64_t t0 = cycles;
        const uint16_t base = static_cast<uint16_t>(page << 8);
        for (uint16_t i = 0; i < 256; ++i)
            oam[i] = read(base + i);
        cycles = t0 + 513 + (t0 & 0x01);
    }
//@LABS-STUB
    // TODO(4): OAM DMA. Copy the 256 bytes of CPU page `page` into oam[],
    // then make the WHOLE operation cost exactly 513 cycles — plus one
    // more when it started on an odd cycle count. This version just adds
    // a flat 513 on top of whatever the reads billed.
    void trigger_oam_dma(uint8_t page) {
        const uint16_t base = static_cast<uint16_t>(page << 8);
        for (uint16_t i = 0; i < 256; ++i)
            oam[i] = read(base + i);
        cycles += 513;  // TODO(4)
    }
//@LABS-END

    // Full CPU-side decode. Cartridge ranges answer through an attached
    // Mapper (exercise 02); everything unlisted reads as open-bus zero.
    uint8_t read(uint16_t addr) {
        ++cycles;
        if (addr < 0x2000) return ram_read(addr);
        if (addr < 0x4000) return ppu_read(addr);
        if (addr == 0x4016 || addr == 0x4017) return controller_read(addr);
        if (cartridge != nullptr && addr >= 0x8000)
            return cartridge->cpu_read(addr);
        return 0;
    }

    void write(uint16_t addr, uint8_t v) {
        ++cycles;
        if (addr < 0x2000) {
            ram_write(addr, v);
        } else if (addr < 0x4000) {
            ppu_write(addr, v);
        } else if (addr == 0x4014) {
            trigger_oam_dma(v);
        } else if (addr == 0x4016 || addr == 0x4017) {
            controller[(addr - 0x4016) & 1].write_strobe(v);
        } else if (cartridge != nullptr && addr >= 0x8000) {
            cartridge->cpu_write(addr, v);
        }
        // APU and remaining IO: accepted and dropped this chapter.
    }

    // Cartridge connector (populated in exercise 02 / 99).
    struct Mapper {
        virtual ~Mapper() = default;
        virtual uint8_t cpu_read(uint16_t) = 0;
        virtual void cpu_write(uint16_t, uint8_t) = 0;
    };
    Mapper* cartridge = nullptr;

private:
    std::array<uint8_t, 0x800> ram_{};
};

}  // namespace nesbus
