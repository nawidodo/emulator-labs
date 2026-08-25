// ct_sequencer_tests — public tests + fixture mode (chapter 45 coding test).
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "labstest.hpp"
#include "sequencer.hpp"

namespace {
bool run_script(cdt::Sequencer& s, const std::string& path,
                std::ostream& log) {
    std::ifstream in(path);
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string op;
        ss >> op;
        if (op == "getstat") s.cmd_getstat();
        else if (op == "setloc") {
            unsigned m, sec, f;
            char c1, c2;
            ss >> m >> c1 >> sec >> c2 >> f;
            if (c1 != ':' || c2 != ':') return false;
            s.cmd_setloc(m, sec, f);
        } else if (op == "init") s.cmd_init();
        else if (op == "pause") s.cmd_pause();
        else if (op == "readn") s.cmd_readn();
        else if (op == "tick") {
            uint64_t n = 0;
            ss >> n;
            s.tick(n);
        } else return false;
    }
    return true;
}
}  // namespace

TEST(ct, init_two_phase_timings) {
    cdt::Sequencer s;
    std::ostringstream log;
    s.set_log(&log);
    s.cmd_init();
    s.tick(1200);
    const auto out = log.str();
    EXPECT_NE(out.find("t=0 int=3"), std::string::npos);
    EXPECT_NE(out.find("t=1200 int=2"), std::string::npos);
}

TEST(ct, pause_invalidates_pending_sectors) {
    cdt::Sequencer s;
    std::ostringstream log;
    s.set_log(&log);
    s.cmd_setloc(0, 2, 0);      // target LBA 0 == current
    s.cmd_readn();
    s.tick(150);                // first sector at t=100+50=150
    EXPECT_EQ(s.current(), 1);
    s.cmd_pause();              // aborts the stream
    s.tick(500);
    EXPECT_EQ(s.current(), 1);  // frozen after pause
}

TEST(ct, seek_distance_uses_leadin_bias) {
    cdt::Sequencer s;
    std::ostringstream log;
    s.set_log(&log);
    s.cmd_setloc(0, 4, 26);     // (4*75+26) - 150 = 176
    EXPECT_EQ(s.target(), 176);
}

int main(int argc, char** argv) {
    if (argc > 3) {
        // Fixture mode: SCRIPT LOG EXPECTED.
        cdt::Sequencer s;
        std::ostringstream log;
        s.set_log(&log);
        if (!run_script(s, argv[1], log)) {
            std::cerr << "error: bad script\n";
            return 2;
        }
        const std::string got = log.str();
        std::ofstream out(argv[2]);
        out << got;
        std::ifstream exp(argv[3]);
        if (!exp) {
            std::cerr << "error: cannot open expected\n";
            return 2;
        }
        std::stringstream expect_buf;
        expect_buf << exp.rdbuf();
        if (got != expect_buf.str()) {
            std::cout << "MISMATCH\n--- got ---\n" << got
                      << "--- want ---\n" << expect_buf.str();
            return 1;
        }
        std::cout << "transcript matches\n";
        return 0;
    }
    ::labstest::run_all("");
    return ::labstest::failures() == 0 ? 0 : 1;
}
