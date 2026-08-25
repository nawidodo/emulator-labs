#pragma once
#include <cstdint>
#include <cstdio>
#include <string>

namespace psx::r3000a {

// Canonical runner trace line (see docs/AUTHORING.md):
//   pc=<hex8> op=<hex8> cyc=<decimal>
// compare_trace.py consumes whitespace-separated key=value tokens.
inline std::string format_trace_line(uint32_t pc, uint32_t instr, uint64_t cycles) {
    char buf[48];
    std::snprintf(buf, sizeof buf, "pc=%08X op=%08X cyc=%llu", pc, instr,
                  static_cast<unsigned long long>(cycles));
    return std::string(buf);
}

}  // namespace psx::r3000a
