#pragma once
//
// ch45 / 91_challenge — scripted CD-ROM session runner.
//
// Script grammar (one command per line, '#' comments):
//   getstat | init | pause | seekl
//   setloc <mm>:<ss>:<ff>        (decimal MSF)
//   readn | reads                ("reads" = double-speed read mode)
//   tick <n>
//
// Log line format (deterministic; consumed by goldens):
//   t=<dec> int=<0..5> resp=<hex bytes joined by '-'> lba=<dec>

#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <cstdio>

#include "../03_read_engine/read_engine.hpp"

namespace cdlc {

using namespace cdrom;

inline void run_script(CdRomController& c, const std::string& path,
                       std::ostream& log) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open script: " + path);

    bool double_speed = false;
    auto emit = [&](uint8_t lvl, const std::vector<uint8_t>& resp,
                    bool with_lba) {
        log << "t=" << c.now() << " int=" << static_cast<unsigned>(lvl)
            << " resp=";
        for (size_t i = 0; i < resp.size(); ++i) {
            if (i) log << '-';
            char b[3];
            std::snprintf(b, sizeof(b), "%02X", resp[i]);
            log << b;
        }
        if (with_lba) log << " lba=" << c.current_lba();
        log << "\n";
    };

    c.set_log_sink([&](uint8_t lvl, const std::vector<uint8_t>& resp) {
        // Data-ready interrupts carry the delivered LBA in the log.
        emit(lvl, resp, lvl == 1);
    });

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string op;
        ss >> op;
        if (op == "getstat") c.issue(kCmdGetStat);
        else if (op == "init") c.issue(kCmdInit);
        else if (op == "pause") c.issue(kCmdPause);
        else if (op == "seekl") cmd_seekl(c);
        else if (op == "setloc") {
            unsigned m, s, f;
            char c1, c2;
            std::string msf;
            ss >> msf;
            std::istringstream ms(msf);
            ms >> m >> c1 >> s >> c2 >> f;
            c.write_param(static_cast<uint8_t>(((m / 10) << 4) | (m % 10)));
            c.write_param(static_cast<uint8_t>(((s / 10) << 4) | (s % 10)));
            c.write_param(static_cast<uint8_t>(((f / 10) << 4) | (f % 10)));
            c.issue(kCmdSetloc);
        } else if (op == "readn" || op == "reads") {
            double_speed = op == "reads";
            start_read(c, double_speed);
        } else if (op == "tick") {
            uint64_t n = 0;
            ss >> n;
            c.tick(n);
        } else {
            throw std::runtime_error("unknown script op: " + op);
        }
    }
}

}  // namespace cdlc
