// Profile matrix harness for the ch06 challenge (curriculum ch6:
// "support multiple CHIP-8 quirk profiles and pass the quirk test").
//
// Runs every quirk-differential fixture under every profile and compares a
// digest of the final machine state against the golden table in
// fixtures_golden.hpp. Exit 0 only when all 15 combinations match.
//
//   ch06_91_matrix            run the matrix, print PASS/FAIL lines
//   ch06_91_matrix --print    authoring aid: dump the observed table

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "chip8.hpp"
#include "fixtures.hpp"
#include "quirks.hpp"
#include "fixtures_golden.hpp"

namespace {

struct Case {
    const char* fixture;
    const uint8_t* rom;
    size_t len;
};

const Case kCases[] = {
    {"q1_shift", ch06::kQ1Shift, sizeof(ch06::kQ1Shift)},
    {"q2_loadstore", ch06::kQ2LoadStore, sizeof(ch06::kQ2LoadStore)},
    {"q3_jump", ch06::kQ3Jump, sizeof(ch06::kQ3Jump)},
    {"q4_draw_wrap", ch06::kQ4DrawWrap, sizeof(ch06::kQ4DrawWrap)},
    {"q5_vfreset", ch06::kQ5VfReset, sizeof(ch06::kQ5VfReset)},
};

const char* kProfiles[] = {"COSMAC_VIP", "CHIP48", "MODERN"};

uint64_t fnv1a(const std::string& s) {
    uint64_t h = 0xCBF29CE484222325ULL;
    for (char c : s) {
        h ^= uint8_t(c);
        h *= 0x100000001B3ULL;
    }
    return h;
}

std::string hex16(uint64_t v) {
    char buf[32];
    snprintf(buf, sizeof buf, "%016llX",
             static_cast<unsigned long long>(v));
    return buf;
}

std::string state_digest(const std::string& profile, const Case& c) {
    const auto q = ch06::profile_by_name(profile);
    ch06::Chip8 cpu;
    cpu.reset(*q);
    cpu.load({c.rom, c.len});
    int guard = 0;
    while (!cpu.halted() && guard++ < 10000) cpu.step();
    char buf[64];
    std::string s;
    for (int r = 0; r < 16; ++r) {
        snprintf(buf, sizeof buf, "V%X=%02X;", r, cpu.v[r]);
        s += buf;
    }
    snprintf(buf, sizeof buf, "I=%03X;PC=%03X;", cpu.i, cpu.pc);
    s += buf;
    return hex16(fnv1a(s));
}

}  // namespace

int main(int argc, char** argv) {
    const bool print = argc > 1 && std::string(argv[1]) == "--print";

    int pass = 0, total = 0;
    for (const auto& c : kCases) {
        for (const char* profile : kProfiles) {
            const std::string got = state_digest(profile, c);
            ++total;
            if (print) {
                std::cout << "{\"" << profile << "\", \"" << c.fixture
                          << "\", \"0x" << got << "\"},\n";
                continue;
            }
            std::string want = ch06::golden_for(profile, c.fixture);
            if (want.rfind("0x", 0) == 0) want.erase(0, 2);
            const bool ok = !want.empty() && want == got;
            std::cout << (ok ? "PASS " : "FAIL ") << profile << "/"
                      << c.fixture << " digest=" << got;
            if (!ok) std::cout << " expected=" << (want.empty() ? "<missing>" : want);
            std::cout << "\n";
            pass += ok ? 1 : 0;
        }
    }
    if (!print)
        std::printf("matrix: %d/%d %s\n", pass, total,
                    pass == total ? "PASS" : "FAIL");
    return print || pass == total ? 0 : 1;
}
