// unseen_mapper.cpp — implement the MBC-X spec from CODING_TEST.md here.
// Signatures are already declared in unseen_mapper.hpp; fill in the
// three @LABS bodies. The constructor is provided.
#include "unseen_mapper.hpp"

namespace mbcx {

MbcX::MbcX(const uint8_t* rom, size_t size)
    : rom_(rom), size_(size), r1_(1), r2_(0) {}

void MbcX::writeReg(uint16_t addr, uint8_t val) {
//@LABS-BEGIN 1
//@LABS-SOLUTION
    if (addr >= 0x2000 && addr < 0x3000) {
        r1_ = val & 0x07;
        if (r1_ == 0) r1_ = 1;
    } else if (addr >= 0x4000 && addr < 0x5000) {
        r2_ = val & 0x01;
    }
//@LABS-STUB
    // TODO(1): $2000-$3FFF sets R1 = val & 0x07 (3-bit bank, 0 -> 1);
    // $4000-$5FFF sets R2 = val & 0x01; every other write is ignored.
    (void)addr;
    (void)val;
//@LABS-END
}

uint8_t MbcX::readRom(uint16_t addr) const {
//@LABS-BEGIN 2
//@LABS-SOLUTION
    if (addr < 0x4000) return addr < size_ ? rom_[addr] : 0xFF;
    if (r2_ != 0) return 0xFF;             // soft open bus
    const size_t off = (addr - 0x4000) + static_cast<size_t>(r1_) * 0x4000;
    return off < size_ ? rom_[off] : 0xFF;
//@LABS-STUB
    // TODO(2): $0000-$3FFF always reads physical bank 0. $4000-$7FFF
    // returns 0xFF while R2 == 1 (soft open bus), otherwise physical
    // bank R1. Out-of-image offsets read 0xFF.
    (void)addr;
    return 0xFF;
//@LABS-END
}

Mapper* makeMapper(const uint8_t* rom, size_t size) {
//@LABS-BEGIN 3
//@LABS-SOLUTION
    if (size < 0x150 || rom[0x147] != kTypeCode) return nullptr;
    return new MbcX(rom, size);
//@LABS-STUB
    // TODO(3): return a new MbcX when header type ($0147) is $BE and the
    // image holds a full header; nullptr otherwise.
    (void)rom;
    (void)size;
    return nullptr;
//@LABS-END
}

}  // namespace mbcx
