#pragma once
#include <cstdint>

#include "../01_daa_rotates/core.hpp"

namespace gb {

// Coding test: the $FF00-page access family. Unseen in this chapter -- the
// ch11 core decoder claims none of these rows, so they trap today.
//
// | E0 nn | ldh (n),A  | write A to $FF00+nn        | 12 cycles |
// | F0 nn | ldh A,(n)  | read $FF00+nn into A       | 12 cycles |
// | E2    | ldh (C),A  | write A to $FF00+C         | 12 cycles |
// | F2    | ldh A,(C)  | read $FF00+C into A        | 12 cycles |
// | 08 nn | ld (nn),SP | store SP little-endian     | 20 cycles |
//
// At this model level the four register forms cost a uniform 12 T-cycles
// (3 M-cycles: fetch operand, internal, memory access); LD (nn),SP costs
// 20 (5 M-cycles). No flags are touched by any of them.

//@LABS-BEGIN 1
//@LABS-SOLUTION
// E0 nn: write A to $FF00+n (the HRAM page).
inline int ldh_e0(Cpu& cpu) {
    const uint8_t n = cpu.fetch8();
    cpu.bus->write(static_cast<uint16_t>(0xFF00 + n), cpu.a);
    return 12;
}
//@LABS-STUB
// TODO(1): implement ldh (n),A [E0]: fetch n, write A to $FF00+n,
// cost 12 cycles.
inline int ldh_e0(Cpu& cpu) {
    (void)cpu;
    return 0;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// F0 nn: read $FF00+n into A.
inline int ldh_f0(Cpu& cpu) {
    const uint8_t n = cpu.fetch8();
    cpu.a = cpu.bus->read(static_cast<uint16_t>(0xFF00 + n));
    return 12;
}
//@LABS-STUB
// TODO(2): implement ldh A,(n) [F0]: fetch n, read $FF00+n into A,
// cost 12 cycles.
inline int ldh_f0(Cpu& cpu) {
    (void)cpu;
    return 0;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// E2 / F2: the register-C forms address $FF00+C -- one opcode byte, no
// operand fetch. Same 12-cycle price at this model level.
inline int ldh_e2(Cpu& cpu) {
    cpu.bus->write(static_cast<uint16_t>(0xFF00 + cpu.c), cpu.a);
    return 12;
}

inline int ldh_f2(Cpu& cpu) {
    cpu.a = cpu.bus->read(static_cast<uint16_t>(0xFF00 + cpu.c));
    return 12;
}
//@LABS-STUB
// TODO(3): implement ldh (C),A [E2] and ldh A,(C) [F2]: address is
// $FF00 + C, cost 12 cycles each, no flags touched.
inline int ldh_e2(Cpu& cpu) {
    (void)cpu;
    return 0;  // wrong on purpose
}
inline int ldh_f2(Cpu& cpu) {
    (void)cpu;
    return 0;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// 08 nn nn: store SP little-endian at nn. The only 20-cycle row of the
// family: operand fetch plus two internal cycles before the writes land.
inline int ld_sp_nn(Cpu& cpu) {
    const uint16_t addr = cpu.fetch16();
    cpu.bus->write(addr, static_cast<uint8_t>(cpu.sp));
    cpu.bus->write(static_cast<uint16_t>(addr + 1),
                   static_cast<uint8_t>(cpu.sp >> 8));
    return 20;
}
//@LABS-STUB
// TODO(4): implement ld (nn),SP [08]: fetch the 16-bit address, store SP
// low byte first, cost 20 cycles.
inline int ld_sp_nn(Cpu& cpu) {
    (void)cpu;
    return 0;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 5
//@LABS-SOLUTION
// Dispatcher for the five-opcode family; everything else returns false so
// the rest of the hook chain (and finally the core decoder) still sees the
// opcode. Installed as a chain Hook exactly like IrqHook.
inline bool ldh_exec(Cpu& cpu, uint8_t op, int& cycles) {
    switch (op) {
        case 0xE0: cycles = ldh_e0(cpu); return true;
        case 0xF0: cycles = ldh_f0(cpu); return true;
        case 0xE2: cycles = ldh_e2(cpu); return true;
        case 0xF2: cycles = ldh_f2(cpu); return true;
        case 0x08: cycles = ld_sp_nn(cpu); return true;
        default: return false;
    }
}

namespace detail {
inline bool ldh_hook_fn(void*, Cpu& cpu, uint8_t op, int& cycles) {
    return ldh_exec(cpu, op, cycles);
}
}  // namespace detail

// Convenience: install as the FIRST hook of a chain.
inline void install_ldh_hook(Cpu& cpu) {
    static Cpu::Hook hook{&detail::ldh_hook_fn, nullptr, nullptr};
    hook.next = cpu.hooks;
    cpu.hooks = &hook;
}
//@LABS-STUB
// TODO(5): wire ldh_exec() over opcodes E0/F0/E2/F2/08 and provide
// install_ldh_hook() as a chain Hook (first slot). Return false for any
// other opcode so the core decoder still handles it.
inline bool ldh_exec(Cpu& cpu, uint8_t op, int& cycles) {
    (void)cpu; (void)op; (void)cycles;
    return false;  // wrong on purpose
}

namespace detail {
inline bool ldh_hook_fn(void*, Cpu&, uint8_t, int&) { return false; }
}  // namespace detail

inline void install_ldh_hook(Cpu&) {}
//@LABS-END

}  // namespace gb
