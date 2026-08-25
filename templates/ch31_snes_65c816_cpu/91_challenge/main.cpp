#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdio>
#include <string>
#include <vector>

// The challenge reuses the 03_execute executor: include path works in
// every generated layout because the whole chapter is generated as one
// tree (see README "Layout" note).
#include "../03_execute/exec.hpp"
namespace {

std::vector<uint8_t> read_bytes(const std::string& path) {
    std::vector<uint8_t> data;
    FILE* f = std::fopen(path.c_str(), "rb");
    EXPECT_TRUE(f != nullptr);
    if (f == nullptr) return data;
    uint8_t buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        data.insert(data.end(), buf, buf + n);
    }
    std::fclose(f);
    return data;
}

std::string run_trace(snescpu::Mem& mem, snescpu::Cpu& cpu,
                      uint64_t max_cycles) {
    std::string out;
    while (cpu.cycles < max_cycles) {
        const uint16_t pc0 = cpu.pc;
        const uint8_t op = mem.read(cpu.k, cpu.pc);
        const int n = snescpu::step(cpu, mem);
        if (n < 0) break;
        cpu.cycles += static_cast<uint64_t>(n);
        out += snescpu::trace_line(cpu, pc0, op);
        out += '\n';
    }
    return out;
}

}  // namespace

TEST(challenge, bank_cross_program_meets_contract) {
    const std::string base = std::string(CHALLENGE_SRC_DIR);
    const auto rom = read_bytes(base + "/roms/challenge.bin");
    EXPECT_EQ(rom.size(), size_t(0x20000));  // exactly two banks

    snescpu::Mem mem;
    snescpu::Cpu cpu;  // reset state: emulation mode, PC=$0000
    mem.load(0x00, 0x0000, rom.data(), 0x10000);
    mem.load(0x01, 0x0000, rom.data() + 0x10000, 0x10000);

    const std::string trace = run_trace(mem, cpu, 1000);

    // Acceptance criteria (CHALLENGE.md):
    EXPECT_EQ(cpu.k, 0x01);                 // crossed into bank 1
    EXPECT_EQ(mem.read(0x00, 0x2000), 0xCD);   // long store, low byte
    EXPECT_EQ(mem.read(0x00, 0x2001), 0xAB);   // long store, high byte
    EXPECT_EQ(mem.read(0x01, 0x3000), 0xEF);   // narrow long store bank 1
    EXPECT_EQ(mem.read(0x01, 0x4007), 0xEF);   // long,X store with X=$07

    // Full golden trace comparison.
    const std::string want = [&] {
        const auto b = read_bytes(base + "/golden/challenge.trace");
        return std::string(b.begin(), b.end());
    }();
    EXPECT_EQ(trace, want);
}
