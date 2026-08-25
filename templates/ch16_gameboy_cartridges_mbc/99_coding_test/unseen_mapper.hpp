// unseen_mapper.hpp — MBC-X interface declarations for the coding test.
// The FULL hardware spec lives in CODING_TEST.md next to this file.
// Implement the three TODO bodies in unseen_mapper.cpp.
#pragma once

#include <cstddef>
#include <cstdint>

namespace mbcx {

constexpr uint16_t kRomBankSize = 0x4000;
constexpr uint8_t kTypeCode = 0xBE;   // header $0147 value for MBC-X carts

class Mapper {  // strategy interface (same shape as exercises 01-04)
 public:
    virtual ~Mapper() = default;
    virtual uint8_t readRom(uint16_t addr) const = 0;   // 0000-7FFF
    virtual uint8_t readRam(uint16_t addr) const = 0;   // A000-BFFF
    virtual void writeReg(uint16_t addr, uint8_t val) = 0;
    virtual void writeRam(uint16_t addr, uint8_t val) = 0;
};

class MbcX : public Mapper {
 public:
    MbcX(const uint8_t* rom, size_t size);

    void writeReg(uint16_t addr, uint8_t val) override;
    uint8_t readRom(uint16_t addr) const override;

    // Spec: MBC-X has no RAM at all — already provided, do not touch.
    uint8_t readRam(uint16_t) const override { return 0xFF; }
    void writeRam(uint16_t, uint8_t) override {}

    // ---- test seams -------------------------------------------------
    uint8_t reg1() const { return r1_; }   // selected ROM bank
    bool reg2() const { return r2_ != 0; } // soft open-bus switch

 private:
    const uint8_t* rom_;
    size_t size_;
    uint8_t r1_;   // resets to 1
    uint8_t r2_;   // resets to 0
};

// Header-driven factory: type $BE -> MbcX, anything else -> nullptr.
Mapper* makeMapper(const uint8_t* rom, size_t size);

}  // namespace mbcx
