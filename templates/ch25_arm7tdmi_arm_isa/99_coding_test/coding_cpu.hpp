#pragma once
#include <cstdint>
#include "arm_cpu.hpp"

namespace arm {

// Unseen-instruction coding test: the ARMv4 "status and swap" family.
//
//   SWP{B} Rd, Rm, [Rn]     cond 00010 B 00 Rn Rd 0000 1001 Rm
//   MRS  Rd, CPSR|SPSR      cond 00010 0 00 1111 Rd 0000 0000 0000
//   MSR  CPSR|SPSR_<f>, Rm  cond 00010 R 10 mask Rd  0000 0000 0000
//   MSR  CPSR|SPSR_<f>, #imm cond 00110 R 10 mask rotate imm8
//
// Field masks: c=control(bit0), s=x(bit1), x=extension(bit2), f=flags(bit3).
// This model implements flags (f -> NZCV) and control (c -> mode/IRQ bits);
// s/x fields exist in the encoding but have no GBA-visible effect here.
struct CodingTestCpu : ArmCpu {
    uint32_t spsr = 0;

    //@LABS-BEGIN 1
//@LABS-SOLUTION
    // SWP / SWPB: read memory at Rn, write Rm there, deliver the OLD value
    // to Rd (zero-extended for the byte form). Modeled cost: 1N + 2S.
    unsigned exec_swap(uint32_t instr) {
        const bool byte = instr & (1 << 22);
        const uint32_t rn = (instr >> 16) & 0xF;
        const uint32_t rd = (instr >> 12) & 0xF;
        const uint32_t rm = instr & 0xF;
        const uint32_t addr = r[rn];
        uint32_t old_val;
        if (byte) {
            old_val = mem[addr & (kMemSize - 1)];
            mem[addr & (kMemSize - 1)] = static_cast<uint8_t>(r[rm]);
        } else {
            old_val = read32(addr);
            write32(addr, r[rm]);
        }
        r[rd] = old_val;
        return 3;
    }
//@LABS-STUB
    unsigned exec_swap(uint32_t instr) {
        // TODO(1): atomic-style swap per the encoding above.
        (void)instr;
        return 3;
    }
//@LABS-END

    //@LABS-BEGIN 2
//@LABS-SOLUTION
    // MRS Rd, CPSR|SPSR: copy status register into Rd.
    unsigned exec_mrs(uint32_t instr) {
        const uint32_t rd = (instr >> 12) & 0xF;
        r[rd] = (instr & (1 << 22)) ? spsr : cpsr;
        return 1;
    }
//@LABS-STUB
    unsigned exec_mrs(uint32_t instr) {
        // TODO(2): select CPSR or SPSR by bit 22, copy into Rd.
        (void)instr;
        return 1;
    }
//@LABS-END

    //@LABS-BEGIN 3
//@LABS-SOLUTION
    // MSR CPSR|SPSR_<fields>, Rm or #imm. Only masked fields change.
    void apply_msr_value(bool spsr_sel, uint32_t mask, uint32_t value) {
        if (spsr_sel) {
            spsr = (spsr & ~mask) | (value & mask);
            return;
        }
        cpsr = (cpsr & ~mask) | (value & mask);
    }

    // Returns true when `instr` belongs to this family and was executed.
    bool maybe_exec_status(uint32_t instr) {
        if ((instr & 0x0FB00FF0) == 0x01000090) {   // SWP/SWPB
            exec_swap(instr);
            return true;
        }
        if ((instr & 0x0FBF0FFF) == 0x010F0000) {   // MRS
            exec_mrs(instr);
            return true;
        }
        if ((instr & 0x0FB00000) == 0x03200000) {   // MSR #imm
            const uint32_t rot = (instr >> 8) & 0xF;
            const auto res =
                shift_ror(2 * rot, instr & 0xFF, c_flag());
            apply_msr_value(instr & (1 << 22), msr_field_mask(instr),
                            res.value);
            return true;
        }
        if ((instr & 0x0FB0F000) == 0x0120F000) {   // MSR Rm
            apply_msr_value(instr & (1 << 22), msr_field_mask(instr),
                            r[instr & 0xF]);
            return true;
        }
        return false;
    }
//@LABS-STUB
    void apply_msr_value(bool spsr_sel, uint32_t mask, uint32_t value) {
        // TODO(3): write only the masked fields of the selected register.
        (void)spsr_sel; (void)mask; (void)value;
    }

    // Returns true when `instr` belongs to this family and was executed.
    bool maybe_exec_status(uint32_t instr) {
        // TODO(3): detect SWP/SWPB, MRS, MSR-imm, MSR-reg by their fixed
        // bit patterns and route each to its handler.
        (void)instr;
        return false;
    }
//@LABS-END

    static uint32_t msr_field_mask(uint32_t instr) {
        uint32_t mask = 0;
        if (instr & (1 << 16)) mask |= 0x000000FFu;   // c: control
        if (instr & (1 << 19)) mask |= 0xFF000000u;   // f: flags (NZCV)
        return mask;
    }

    unsigned step() {
        const uint32_t pc = r[15];
        const uint32_t instr = read32(pc);
        if ((instr >> 28) != 0xE && !cond_pass(instr >> 28, cpsr)) {
            r[15] += 4;
            return 1;
        }
        if (maybe_exec_status(instr)) {
            r[15] += 4;
            return 1;
        }
        return ArmCpu::step();
    }
};

}  // namespace arm
