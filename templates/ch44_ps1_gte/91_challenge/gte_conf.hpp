#pragma once
//
// ch44 / 91_challenge — GTE conformance harness.
//
// Input snapshot format (text, one record per RUN):
//   OP=RTPS|RTPT|NCDS|MVMVA|AVSZ3 SF=0|1 LM=0|1 [MAT=n VEC=n ADD=0|1]
//   REG=<idx>:<hex8> ...            (data-space writes, applied in order)
//   CREG=<idx>:<hex8> ...           (control-space writes, incl. FLAG)
//   RUN
//
// After each RUN the runner emits ONE line:
//   out mac0=<hex8> mac1=<hex8> mac2=<hex8> mac3=<hex8>
//       ir0=<hex4> ir1=<hex4> ir2=<hex4> ir3=<hex4>
//       sz3=<hex4> sxy2=<hex8> flag=<hex8>
//
// Registers persist between records (stateful conformance run).

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "../03_lighting/lighting.hpp"

namespace gteconf {

struct Snapshot {
    std::string op = "RTPS";
    bool sf = true, lm = false;
    unsigned mat = 0, vec = 0;
    bool add12 = false;

    void apply(gte::Cop2& g) const {
        if (op == "RTPS") {
            gte::rtps(g, 0, lm);
        } else if (op == "RTPT") {
            gte::rtpt(g);
        } else if (op == "NCDS") {
            gte::ncds(g, lm);
        } else if (op == "MVMVA") {
            gte::Command c{};
            c.op = 0x12; c.sf = sf; c.lm = lm;
            c.mat = mat; c.vec = vec; c.add12 = add12;
            gte::mvmva(g, c);
        } else if (op == "AVSZ3") {
            gte::avsz3(g);
        }
    }
};

inline std::string result_line(const gte::Cop2& g) {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "out mac0=%08X mac1=%08X mac2=%08X mac3=%08X "
                  "ir0=%04X ir1=%04X ir2=%04X ir3=%04X sz3=%04X "
                  "sxy2=%08X flag=%08X",
                  g.rd(24), g.rd(25), g.rd(26), g.rd(27),
                  g.rd(8) & 0xFFFFu, g.rd(9) & 0xFFFFu,
                  g.rd(10) & 0xFFFFu, g.rd(11) & 0xFFFFu,
                  g.rd(19) & 0xFFFFu, g.rd(14), g.flag());
    return buf;
}

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Runs a whole conformance file; appends one output line per RUN to `out`.
inline bool run_file(const std::string& path, std::ostream& out) {
    std::ifstream in(path);
    if (!in) return false;
    gte::Cop2 g;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string key;
        Snapshot snap;              // per-record op selection
        bool ran = false;
        while (ss >> key) {
            if (key == "RUN") {
                snap.apply(g);
                out << result_line(g) << '\n';
                ran = true;
                continue;
            }
            const auto eq = key.find('=');
            if (eq == std::string::npos) continue;
            const std::string k = key.substr(0, eq);
            const std::string v = key.substr(eq + 1);
            auto u32 = [](const std::string& s) {
                return static_cast<uint32_t>(std::stoul(s, nullptr, 16));
            };
            if (k == "OP") snap.op = v;
            else if (k == "SF") snap.sf = v == "1";
            else if (k == "LM") snap.lm = v == "1";
            else if (k == "MAT") snap.mat = u32(v);
            else if (k == "VEC") snap.vec = u32(v);
            else if (k == "ADD") snap.add12 = v == "1";
            else if (k == "REG") {
                const auto colon = v.find(':');
                g.wd(static_cast<unsigned>(
                         std::stoul(v.substr(0, colon), nullptr, 0)),
                     u32(v.substr(colon + 1)));
            } else if (k == "CREG") {
                const auto colon = v.find(':');
                g.wc(static_cast<unsigned>(
                         std::stoul(v.substr(0, colon), nullptr, 0)),
                     u32(v.substr(colon + 1)));
            }
        }
        (void)ran;
    }
    return true;
}
//@LABS-STUB
// TODO(1): parse the documented snapshot format and emit one result_line
// per RUN. Registers persist across records. Return false only when the
// file cannot be opened.
bool run_file(const std::string& path, std::ostream& out) {
    (void)path; (void)out;
    return false;  // wrong on purpose
}
//@LABS-END

}  // namespace gteconf
