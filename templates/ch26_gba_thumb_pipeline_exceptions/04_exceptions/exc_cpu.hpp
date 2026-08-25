#pragma once
#include <cstdint>
#include "conditions.hpp"
#include "shifter.hpp"
#include "thumb_decoder.hpp"

using arm::FLAG_N;
using arm::FLAG_Z;
using arm::FLAG_C;
using arm::FLAG_V;

namespace exc {

// ARMv4 CPU modes (low 5 bits of CPSR) and their banked registers.
//
// | Mode | Bits | Banked                        | Vector |
// |------|------|-------------------------------|--------|
// | User |10000 | none                          | —      |
// | FIQ  |10001 | r8-r12, r13, r14, SPSR        | 0x1C   |
// | IRQ  |10010 | r13, r14, SPSR                | 0x18   |
// | SVC  |10011 | r13, r14, SPSR                | 0x08   |
// | ABT  |10111 | r13, r14, SPSR                | 0x10   |
// | UND  |11011 | r13, r14, SPSR                | 0x04   |
// | SYS  |11111 | r13, r14 (no SPSR)            | —      |
//
// This model implements the four modes a GBA game actually touches:
// User, SVC (SWI), IRQ and FIQ. FIQ additionally banks r8-r12, which is
// why FIQ handlers can run without saving those registers.
enum CpuMode : uint32_t {
    kUser = 0x10,
    kFIQ  = 0x11,
    kIRQ  = 0x12,
    kSvc  = 0x13,
};

struct Banked {
    uint32_t r13 = 0;
    uint32_t r14 = 0;
    uint32_t spsr = 0;
};

// Exception entry/return model over the minimal ARM executor.
//
// Entry (enter_exception):
//   1. SPSR_<new> = CPSR
//   2. switch to the new mode; PC = vector; LR_<new> = lr_value
//   3. mask per the vector table (IRQ: I; FIQ: F+I)
// Return (exception_return):
//   CPSR = SPSR_<cur>, banks swap back, PC = LR_<cur> - ret_offset.
//   SWI/UNDEF return with MOVS PC, LR (ret_offset 0); IRQ/FIQ with
//   SUBS PC, LR, #4 because the saved LR points one pipeline slot past
//   the interrupted instruction in this 3-stage model.
struct ExceptionCpu {
    static constexpr uint32_t kMemSize = 64 * 1024;
    uint8_t mem[kMemSize] = {};
    uint32_t r[16] = {};
    uint32_t cpsr = 0x00000010;           // user mode, I/F clear
    bool t = false;

    CpuMode mode = kUser;
    Banked svc{}, irq{}, fiq{};
    uint32_t fiq_shadow[5] = {0, 0, 0, 0, 0};   // FIQ's private r8-r12

    uint32_t instr_addr = 0;

    static bool flag_n(uint32_t v) { return v >> 31; }
    static bool flag_z(uint32_t v) { return v == 0; }

    uint32_t read32(uint32_t addr) const {
        const uint32_t m = kMemSize - 1, a = addr & m;
        return mem[a] | (mem[(a + 1) & m] << 8) | (mem[(a + 2) & m] << 16) |
               (mem[(a + 3) & m] << 24);
    }
    void write32(uint32_t addr, uint32_t v) {
        const uint32_t m = kMemSize - 1, a = addr & m;
        mem[a] = v; mem[(a + 1) & m] = v >> 8;
        mem[(a + 2) & m] = v >> 16; mem[(a + 3) & m] = v >> 24;
    }

    static uint32_t mode_bits(CpuMode m) { return m; }

    //@LABS-BEGIN 1
//@LABS-SOLUTION
    // Enter an exception: bank state, mask, and vector. `lr_value` is the
    // address the handler must eventually return to (possibly adjusted by
    // ret_offset at return time).
    void enter_exception(CpuMode new_mode, uint32_t vector,
                         uint32_t lr_value, bool disable_fiq) {
        Banked& b = new_mode == kSvc ? svc : (new_mode == kIRQ ? irq : fiq);
        b.spsr = cpsr;
        if (new_mode == kFIQ) {
            // Swap the fast-interrupt register shadow in.
            for (int i = 0; i < 5; ++i) {
                fiq_shadow[i] = r[8 + i];
                r[8 + i] = 0;                  // FIQ set starts zeroed here
            }
        }
        mode = new_mode;
        b.r14 = lr_value;
        cpsr &= ~0x1Fu;
        cpsr |= static_cast<uint32_t>(new_mode) | (1u << 7);   // I bit
        if (disable_fiq) cpsr |= 1u << 6;                      // F bit
        r[15] = vector;
    }
//@LABS-STUB
    void enter_exception(CpuMode new_mode, uint32_t vector,
                         uint32_t lr_value, bool disable_fiq) {
        // TODO(1): save CPSR into the target mode's SPSR, store lr_value in
        // its banked R14, switch the mode bits of CPSR, force the IRQ (and
        // for FIQ also the FIQ) mask, and vector the PC. FIQ swaps r8-r12.
        (void)new_mode; (void)vector; (void)lr_value; (void)disable_fiq;
    }
//@LABS-END

    //@LABS-BEGIN 2
//@LABS-SOLUTION
    // SWI: supervisor call. Enters SVC at vector 0x08 with LR = the
    // instruction after the SWI (instr_addr + 4); no extra masking beyond
    // the standard IRQ disable-free entry per the lecture table — but we
    // do keep I untouched, matching ARMv4T hardware where SWI is itself
    // interruptible.
    unsigned swi() {
        enter_exception(kSvc, 0x08, instr_addr + 4, false);
        cpsr &= ~(1u << 7);
        return 3;                              // refetch after vectoring
    }
//@LABS-STUB
    unsigned swi() {
        // TODO(2): enter SVC mode at vector 0x08 linking to instr_addr+4.
        return 1;
    }
//@LABS-END

    //@LABS-BEGIN 3
//@LABS-SOLUTION
    // External interrupt pins. Recognized only when not masked. The saved
    // LR points one slot PAST the next instruction (classic +4), so the
    // handler returns with SUBS PC, LR, #4 — see exception_return.
    bool irq_line() {
        if (cpsr & (1 << 7)) return false;     // masked: stay pending
        enter_exception(kIRQ, 0x18, r[15] + 4, false);
        return true;
    }
    bool fiq_line() {
        if (cpsr & (1 << 6)) return false;     // F mask
        enter_exception(kFIQ, 0x1C, r[15] + 4, true);
        return true;
    }
//@LABS-STUB
    bool irq_line() {
        // TODO(3): when the I mask is clear, enter IRQ mode at vector
        // 0x18 with LR = PC + 4; return whether it was recognized.
        return false;
    }
    bool fiq_line() {
        // TODO(3): same for FIQ at vector 0x1C with the F mask check;
        // FIQ entry additionally disables both interrupts.
        return false;
    }
//@LABS-END

    //@LABS-BEGIN 4
//@LABS-SOLUTION
    // Return from the current exception mode. `ret_offset` is subtracted
    // from the banked LR: 0 for SWI (MOVS PC, LR), 4 for IRQ/FIQ
    // (SUBS PC, LR, #4). CPSR is restored from SPSR, undoing the mode
    // change and any masks the entry applied; FIQ swaps r8-r12 back.
    void exception_return(unsigned ret_offset) {
        const bool from_fiq = mode == kFIQ;
        Banked& b = mode == kSvc ? svc : (mode == kIRQ ? irq : fiq);
        const uint32_t spsr = b.spsr;
        const uint32_t pc = b.r14 - ret_offset;
        if (from_fiq) {
            for (int i = 0; i < 5; ++i) {
                const uint32_t cur = r[8 + i];
                r[8 + i] = fiq_shadow[i];
                fiq_shadow[i] = cur;
            }
        }
        cpsr = (cpsr & ~0xF00000FFu) | (spsr & 0xF00000FFu);
        mode = static_cast<CpuMode>(cpsr & 0x1F);
        r[15] = pc;
    }
//@LABS-STUB
    void exception_return(unsigned ret_offset) {
        // TODO(4): restore CPSR from the current mode's SPSR, swap the
        // r8-r12 shadow back out of FIQ, and jump to banked LR minus
        // ret_offset.
        (void)ret_offset;
    }
//@LABS-END

    // ---- Minimal ARM executor so SWI can be driven through real code ----
    unsigned exec_arm_dp(uint32_t instr) {
        const uint32_t op = (instr >> 21) & 0xF;
        const uint32_t rn = (instr >> 16) & 0xF, rd = (instr >> 12) & 0xF;
        uint32_t op2;
        if (instr & (1 << 25)) {
            const uint32_t rot = ((instr >> 8) & 0xF) * 2 % 32;
            op2 = rot ? ((instr & 0xFF) >> rot) | ((instr & 0xFF) << (32 - rot))
                      : (instr & 0xFF);
        } else {
            op2 = r[instr & 0xF];
        }
        uint32_t res = 0;
        switch (op) {
        case 0x4: res = r[rn] + op2; break;         // ADD
        case 0xD: res = op2; break;                 // MOV
        default: break;
        }
        if (rd != 15 && op != 0x8 && op != 0xA) r[rd] = res;
        return 1;
    }

    unsigned step() {
        instr_addr = r[15];
        const uint32_t instr = read32(instr_addr);
        r[15] += 4;                                // ARM fetch stride
        if ((instr & 0x0F000000) == 0x0F000000) return swi();
        if ((instr & 0x0FFFFFF0) == 0x012FFF10) {
            const uint32_t target = r[instr & 0xF];
            t = target & 1;
            r[15] = target & ~1u;
            return 3;
        }
        if ((instr & 0x0C000000) == 0x00000000)
            return exec_arm_dp(instr);
        return 1;
    }
};

}  // namespace exc
