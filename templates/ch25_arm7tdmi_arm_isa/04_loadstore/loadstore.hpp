#pragma once
#include <cstdint>
#include "conditions.hpp"

namespace arm {

// Single load/store unit over a flat little-endian scratch memory.
// Implements LDR/STR/LDRB/STRB in the A1 (immediate offset) and A2
// (register offset) encodings, with pre/post indexing and writeback.
struct LoadStoreCpu {
    static constexpr uint32_t kMemSize = 64 * 1024;
    uint8_t mem[kMemSize] = {};
    uint32_t r[16] = {};
    uint32_t cpsr = 0;

    //@LABS-BEGIN 1
//@LABS-SOLUTION
    // Compute the transfer address and apply writeback.
    //   U (bit23): add offset; P (bit24): pre-index; W (bit21): writeback.
    //   Post-index always writes back Rn += offset regardless of W.
    uint32_t transfer_address(uint32_t instr) {
        const uint32_t rn = (instr >> 16) & 0xF;
        const bool u = instr & (1 << 23);
        const bool pre = instr & (1 << 24);
        const bool wb = instr & (1 << 21);
        const int32_t offset =
            (instr & (1 << 25))
                ? static_cast<int32_t>(r[instr & 0xF])           // register off
                : static_cast<int32_t>(instr & 0xFFF);           // imm12 off
        const uint32_t base = r[rn];
        const uint32_t addr = u ? base + static_cast<uint32_t>(offset)
                                : base - static_cast<uint32_t>(offset);
        if (!pre || wb) r[rn] = addr;
        return pre ? addr : base;
    }
//@LABS-STUB
    uint32_t transfer_address(uint32_t instr) {
        // TODO(1): decode U/P/W and the immediate/register offset, compute
        // the access address, and apply writeback for [Rn,#o]! and post-index.
        (void)instr;
        return 0;
    }
//@LABS-END

    void store_word(uint32_t addr, uint32_t value) {
        const uint32_t m = kMemSize - 1;
        mem[addr & m] = value & 0xFF;
        mem[(addr + 1) & m] = (value >> 8) & 0xFF;
        mem[(addr + 2) & m] = (value >> 16) & 0xFF;
        mem[(addr + 3) & m] = (value >> 24) & 0xFF;
    }

    //@LABS-BEGIN 2
//@LABS-SOLUTION
    uint32_t load_word_aligned(uint32_t addr) {
        const uint32_t m = kMemSize - 1;
        const uint32_t a = addr & m;
        return mem[a] | (mem[(a + 1) & m] << 8) | (mem[(a + 2) & m] << 16) |
               (mem[(a + 3) & m] << 24);
    }

    uint32_t load_word(uint32_t addr) {
        const uint32_t aligned = load_word_aligned(addr & ~3u);
        const uint32_t rot = (addr & 3) * 8;
        return (aligned >> rot) | (rot ? aligned << (32 - rot) : 0);
    }
//@LABS-STUB
    uint32_t load_word(uint32_t addr) {
        // TODO(2): little-endian read of 4 bytes, rotated by (addr&3)*8.
        (void)addr;
        return 0;
    }
//@LABS-END

    //@LABS-BEGIN 3
//@LABS-SOLUTION
    uint8_t load_byte(uint32_t addr) { return mem[addr & (kMemSize - 1)]; }
    void store_byte(uint32_t addr, uint8_t value) {
        mem[addr & (kMemSize - 1)] = value;
    }
//@LABS-STUB
    uint8_t load_byte(uint32_t addr) {
        // TODO(3): single-byte read; zero-extended into Rd by the caller.
        (void)addr;
        return 0;
    }
    void store_byte(uint32_t addr, uint8_t value) {
        // TODO(3): single-byte write.
        (void)addr; (void)value;
    }
//@LABS-END

    //@LABS-BEGIN 4
//@LABS-SOLUTION
    // Execute one LDR/STR-family instruction (cond already checked).
    // Returns cycles consumed: 1N + 1S on this model.
    unsigned exec_ls(uint32_t instr) {
        const bool l = instr & (1 << 20);
        const bool b = instr & (1 << 22);
        const uint32_t rd = (instr >> 12) & 0xF;
        const uint32_t addr = transfer_address(instr);
        if (l)
            r[rd] = b ? load_byte(addr) : load_word(addr);
        else if (b)
            store_byte(addr, static_cast<uint8_t>(r[rd]));
        else
            store_word(addr, r[rd]);
        return 2;
    }
//@LABS-STUB
    unsigned exec_ls(uint32_t instr) {
        // TODO(4): dispatch on L/B, get the address, move the data.
        (void)instr;
        return 2;
    }
//@LABS-END
};

}  // namespace arm
