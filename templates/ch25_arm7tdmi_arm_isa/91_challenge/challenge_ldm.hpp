#pragma once
#include <cstdint>
#include "arm_cpu.hpp"

namespace arm {

// Load/store multiple for the chapter challenge: LDM/STM in increment-after
// (IA) and decrement-before (DB) flavors, with optional base writeback.
// Lowest register always maps to the lowest address.

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Number of registers set in the list.
inline unsigned reg_count(uint32_t list) {
    unsigned n = 0;
    while (list) { n += list & 1; list >>= 1; }
    return n;
}
//@LABS-STUB
inline unsigned reg_count(uint32_t list) {
    // TODO(1): popcount of the 16-bit register list.
    (void)list;
    return 0;
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Execute one LDM/STM word (cond already checked by caller).
// Encoding: cond 100 P U S W L Rn reglist16.
//   IA: P=0,U=1 ; DB: P=1,U=0. Returns (n+1)S + 1N cycles.
unsigned exec_block(ArmCpu& cpu, uint32_t instr) {
    const bool up = instr & (1 << 23);
    const bool wb = instr & (1 << 21);
    const bool load = instr & (1 << 20);
    const uint32_t rn = (instr >> 16) & 0xF;
    const uint32_t list = instr & 0xFFFF;
    const unsigned n = reg_count(list);
    uint32_t addr = cpu.r[rn];
    if (!up) addr -= n * 4;                    // DB starts below the block

    const uint32_t wb_base = up ? addr + n * 4 : addr;
    for (uint32_t rg = 0; rg < 16; ++rg) {
        if (!(list & (1u << rg))) continue;
        if (load)
            cpu.r[rg] = cpu.read32(addr);
        else
            cpu.write32(addr, cpu.r[rg]);
        addr += 4;
    }
    if (wb && !(load && (list & (1u << rn)))) cpu.r[rn] = wb_base;
    return n + 2;
}
//@LABS-STUB
unsigned exec_block(ArmCpu& cpu, uint32_t instr) {
    // TODO(2): walk the register list lowest-to-highest, transferring words
    // at ascending addresses; DB begins n*4 below the base; apply
    // writeback unless loading back into the base register itself.
    (void)cpu; (void)instr;
    return 0;
}
//@LABS-END

// Challenge core: adds LDM/STM (family bits 27-25 == 100) to the dispatch.
struct ChallengeCpu : ArmCpu {
    unsigned step() {
        const uint32_t instr = read32(r[15]);
        if ((instr >> 28) != 0xE && !cond_pass(instr >> 28, cpsr)) {
            r[15] += 4;                        // condition failed: 1S
            return 1;
        }
        if ((instr & 0x0E000000) == 0x08000000) {
            const unsigned cycles = exec_block(*this, instr);
            r[15] += 4;                        // block ops are not branches here
            return cycles;
        }
        return ArmCpu::step();                 // everything else as usual
    }
};

}  // namespace arm
