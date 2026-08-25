#pragma once
#include <cstdint>
#include <cstdio>
#include <string>

#include "../03_overflow_reload/timer_irq.hpp"

namespace gbfmt {

// Exercise 13.91 -- canonical interrupt-log and final-state formats.
//
// Interrupt log (--trace FILE), one line per event, then exactly one
// final `state` line:
//   cyc=<n> tima_overflow
//   cyc=<n> irq vector=<hh> ime=<0|1>
// Keys are lowercase and whitespace-separated; hex digits uppercase
// (vector is two nibbles); cyc decimal; ime/halted/trap print as 0/1.
// The SAME `state` line is what --hash-frame writes (the CPU-phase
// convention for golden hashes without a framebuffer).

inline std::string overflow_line(uint64_t cyc) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "cyc=%llu tima_overflow\n",
                  static_cast<unsigned long long>(cyc));
    return buf;
}

inline std::string irq_line(uint64_t cyc, uint16_t vector, bool ime) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "cyc=%llu irq vector=%02X ime=%d\n",
                  static_cast<unsigned long long>(cyc), vector, ime ? 1 : 0);
    return buf;
}

inline std::string state_line(const gb::TimerMachine& m) {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "state af=%04X bc=%04X de=%04X hl=%04X sp=%04X pc=%04X "
                  "cyc=%llu halted=%d trap=%d if=%02X ie=%02X "
                  "tima=%02X tma=%02X tac=%02X\n",
                  m.cpu.af(), m.cpu.bc(), m.cpu.de(), m.cpu.hl(), m.cpu.sp,
                  m.cpu.pc, static_cast<unsigned long long>(m.cyc),
                  m.cpu.halted ? 1 : 0, m.cpu.trap ? 1 : 0,
                  m.ctl.flags, m.ctl.enabled,
                  m.timer.tima, m.timer.tma, m.timer.read_tac());
    return buf;
}

}  // namespace gbfmt
