#pragma once
#include <cstdint>

#include "../03_ld_alu/cpu.hpp"

namespace gbtest {

// LDH opcode family handlers. Each returns cycles consumed and reports
// "handled" through ldh_exec(). All use the $FF00-high address window.
inline constexpr uint16_t kHighBase = 0xFF00;

//@LABS-BEGIN 1
//@LABS-SOLUTION
// E0 nn: ldh (n),A -- write A to $FF00+n.
inline int ldh_e0(gb::Cpu& cpu) {
    const uint8_t n = cpu.fetch8();
    cpu.bus->write(static_cast<uint16_t>(kHighBase + n), cpu.a);
    return 12;
}
//@LABS-STUB
// TODO(1): implement ldh (n),A (opcode E0): fetch n, write A to $FF00+n,
// cost 12 cycles.
inline int ldh_e0(gb::Cpu& cpu) {
    (void)cpu;
    return 0;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// F0 nn: ldh A,(n); E2/F2: the (C) forms cost only 8 cycles.
inline int ldh_f0(gb::Cpu& cpu) {
    const uint8_t n = cpu.fetch8();
    cpu.a = cpu.bus->read(static_cast<uint16_t>(kHighBase + n));
    return 12;
}

inline int ldh_e2(gb::Cpu& cpu) {
    cpu.bus->write(static_cast<uint16_t>(kHighBase + cpu.c), cpu.a);
    return 8;
}

inline int ldh_f2(gb::Cpu& cpu) {
    cpu.a = cpu.bus->read(static_cast<uint16_t>(kHighBase + cpu.c));
    return 8;
}
//@LABS-STUB
// TODO(2): implement ldh A,(n) [F0, 12 cycles], ldh (C),A [E2, 8 cycles]
// and ldh A,(C) [F2, 8 cycles].
inline int ldh_f0(gb::Cpu& cpu) {
    (void)cpu;
    return 0;  // wrong on purpose
}
inline int ldh_e2(gb::Cpu& cpu) {
    (void)cpu;
    return 0;  // wrong on purpose
}
inline int ldh_f2(gb::Cpu& cpu) {
    (void)cpu;
    return 0;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// SP-relative arithmetic shared by E8 (result -> SP) and F8 (result -> HL).
// Flags: Z=0 N=0; H = carry out of bit 3, C = carry out of bit 7.
inline uint16_t sp_plus_e(gb::Cpu& cpu, uint8_t raw_e) {
    // Hardware computes carries on the RAW unsigned offset byte, even when
    // the effective displacement is negative (two's complement).
    const auto offset = static_cast<int8_t>(raw_e);
    const uint32_t sum =
        static_cast<uint32_t>(cpu.sp) + static_cast<uint32_t>(offset);
    cpu.set_z(false);
    cpu.set_n(false);
    cpu.set_h(((cpu.sp & 0xF) + (raw_e & 0xF)) > 0xF);
    cpu.set_c(((cpu.sp & 0xFF) + raw_e) > 0xFF);
    return static_cast<uint16_t>(sum);
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// E8 dd: add SP,e (16 cycles). F8 dd: ld HL,SP+e (12 cycles).
inline int ldh_e8(gb::Cpu& cpu) {
    const uint8_t raw = cpu.fetch8();
    cpu.sp = sp_plus_e(cpu, raw);
    return 16;
}

inline int ldh_f8(gb::Cpu& cpu) {
    const uint8_t raw = cpu.fetch8();
    cpu.set_hl(sp_plus_e(cpu, raw));
    return 12;
}
//@LABS-STUB
// TODO(4): implement add SP,e [E8, 16 cycles] and ld HL,SP+e [F8, 12
// cycles] using sp_plus_e().
inline int ldh_e8(gb::Cpu& cpu) {
    (void)cpu;
    return 0;  // wrong on purpose
}
inline int ldh_f8(gb::Cpu& cpu) {
    (void)cpu;
    return 0;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 5
//@LABS-SOLUTION
// Dispatcher installed via cpu.extra_exec. Handles exactly the six LDH
// opcodes; everything else falls back to the base decoder (and traps there
// if unimplemented).
inline bool ldh_exec(gb::Cpu& cpu, uint8_t op, int& cycles) {
    switch (op) {
        case 0xE0: cycles = ldh_e0(cpu); return true;
        case 0xF0: cycles = ldh_f0(cpu); return true;
        case 0xE2: cycles = ldh_e2(cpu); return true;
        case 0xF2: cycles = ldh_f2(cpu); return true;
        case 0xE8: cycles = ldh_e8(cpu); return true;
        case 0xF8: cycles = ldh_f8(cpu); return true;
        default: return false;
    }
}
//@LABS-STUB
// TODO(5): wire up the dispatcher for opcodes E0/E2/E8/F0/F2/F8; return
// false for anything else so the base decoder still traps cleanly.
inline bool ldh_exec(gb::Cpu& cpu, uint8_t op, int& cycles) {
    (void)cpu;
    (void)op;
    (void)cycles;
    return false;
}
//@LABS-END

}  // namespace gbtest
