#define LABSTEST_MAIN
#include "labstest.hpp"

#include <sstream>
#include <string>
#include <vector>

#include "chip8.hpp"
#include "compat_answer.hpp"
#include "quirks.hpp"
#include "trace.hpp"

namespace {

using ch06::Chip8;
using ch06::Chip8Quirks;

// The failing program: saves V0..V3 to 0x400, reloads them, then branches
// on the reloaded V3. Under COSMAC_VIP (the variant it was written for)
// FX55/FX65 leave I alone; under MODERN/CHIP48 they advance I, so the
// reload reads zeros and the branch behaves differently.
const uint8_t kRom[] = {
    0x63, 0x12,  // 200: LD   V3, 0x12
    0xA4, 0x00,  // 202: LD   I, 0x400
    0xF3, 0x55,  // 204: LD   [I], V3
    0xF3, 0x65,  // 206: LD   V3, [I]
    0x33, 0x12,  // 208: SE   V3, 0x12
    0x60, 0xFF,  // 20A: LD   V0, 0xFF  <- skipped when V3==12 holds
    0x12, 0x0E,  // 20C: JP   20E       -> program end
};

std::vector<std::string> full_trace(const std::string& profile) {
    const auto q = ch06::profile_by_name(profile);
    Chip8 cpu;
    cpu.reset(*q);
    cpu.load(kRom);
    std::ostringstream os;
    ch06::TraceWriter tw(os, true);
    int guard = 0;
    while (!cpu.halted() && guard++ < 1000) tw.log(cpu), cpu.step();
    std::vector<std::string> lines;
    std::string line;
    std::istringstream is(os.str());
    while (std::getline(is, line)) lines.push_back(line);
    return lines;
}

int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Returns the pc= token of the first line where reference and suspect trace
// disagree, or -1 if the traces are identical.
long first_divergence_pc(const std::vector<std::string>& ref,
                         const std::vector<std::string>& suspect) {
    for (size_t k = 0; k < ref.size() && k < suspect.size(); ++k) {
        if (ref[k] == suspect[k]) continue;
        const size_t p = ref[k].find("pc=");
        long pc = 0;
        for (size_t c = p + 3; c < ref[k].size() && hex_val(ref[k][c]) >= 0; ++c)
            pc = pc * 16 + hex_val(ref[k][c]);
        return pc;
    }
    return -1;
}

}  // namespace

TEST(hunt, wrong_profile_diverges_from_reference) {
    const auto ref = full_trace("COSMAC_VIP");
    const auto bad = full_trace("MODERN");
    EXPECT_NE(ref.size(), bad.size());  // different paths through the ROM
}

TEST(hunt, recorded_answer_matches_true_first_divergence) {
    const auto ref = full_trace("COSMAC_VIP");
    const auto bad = full_trace("MODERN");
    EXPECT_NE(ch06::kFirstDivergencePc, 0);
    EXPECT_EQ(long(ch06::kFirstDivergencePc),
              first_divergence_pc(ref, bad));
}
