// Headless runner for the ch31 65C816 executor (curriculum section 52
// CLI shape). The ROM image is mapped sequentially into banks starting
// at $00:$0000 (each 64 KiB chunk fills one bank); execution starts in
// emulation mode with PC=$0000 and stops on BRK or the --cycles budget.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "exec.hpp"

namespace {

void usage() {
    std::printf(
        "ch31 65C816 headless runner\n"
        "usage: ch31_03_exec_runner --rom PATH [--cycles N] [--trace FILE]\n"
        "                          [--headless] [--frames N]\n"
        "                          [--hash-frame FILE] [--input-file FILE]\n"
        "\n"
        "--rom PATH       raw program image, bank 0 first, PC=$0000\n"
        "--cycles N       cycle budget (default 100000)\n"
        "--trace FILE     write one trace line per executed instruction\n"
        "--headless       accepted for CLI parity (always headless)\n"
        "--frames N       accepted for CLI parity (no video in this system)\n"
        "--hash-frame F   accepted for CLI parity (no framebuffer)\n"
        "--input-file F   accepted for CLI parity (no controller input)\n");
}

std::vector<uint8_t> read_file(const std::string& path) {
    std::vector<uint8_t> data;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return data;
    uint8_t buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        data.insert(data.end(), buf, buf + n);
    }
    std::fclose(f);
    return data;
}

}  // namespace

int main(int argc, char** argv) {
    std::string rom_path, trace_path;
    uint64_t budget = 100000;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help") {
            usage();
            return 0;
        } else if (arg == "--rom" && i + 1 < argc) {
            rom_path = argv[++i];
        } else if (arg == "--trace" && i + 1 < argc) {
            trace_path = argv[++i];
        } else if (arg == "--cycles" && i + 1 < argc) {
            budget = std::strtoull(argv[++i], nullptr, 0);
        } else if (arg == "--headless") {
            // always headless
        } else if ((arg == "--frames" || arg == "--hash-frame" ||
                    arg == "--input-file") &&
                   i + 1 < argc) {
            ++i;  // CLI-parity flags: nothing to do for a CPU-only system
        } else {
            std::fprintf(stderr, "unknown/incomplete argument: %s\n",
                         arg.c_str());
            usage();
            return 2;
        }
    }

    if (rom_path.empty()) {
        std::fprintf(stderr, "error: --rom is required\n");
        usage();
        return 2;
    }
    const auto rom = read_file(rom_path);
    if (rom.empty()) {
        std::fprintf(stderr, "error: cannot read rom '%s'\n",
                     rom_path.c_str());
        return 2;
    }

    snescpu::Mem mem;
    snescpu::Cpu cpu;
    size_t off = 0;
    uint8_t bank = 0;
    while (off < rom.size() && bank <= 0xFF) {
        const size_t chunk =
            rom.size() - off < 0x10000 ? rom.size() - off : size_t(0x10000);
        mem.load(bank++, 0x0000, rom.data() + off, chunk);
        off += chunk;
    }

    FILE* tf = trace_path.empty() ? nullptr
                                  : std::fopen(trace_path.c_str(), "w");
    if (!trace_path.empty() && tf == nullptr) {
        std::fprintf(stderr, "error: cannot write trace '%s'\n",
                     trace_path.c_str());
        return 2;
    }

    bool halted = false;
    while (cpu.cycles < budget) {
        const uint16_t pc0 = cpu.pc;
        const uint8_t op = mem.read(cpu.k, cpu.pc);
        const int n = snescpu::step(cpu, mem);
        if (n < 0) {
            halted = true;
            break;
        }
        cpu.cycles += static_cast<uint64_t>(n);
        if (tf != nullptr) {
            std::fprintf(tf, "%s\n",
                         snescpu::trace_line(cpu, pc0, op).c_str());
        }
    }
    if (tf != nullptr) std::fclose(tf);

    std::printf("halted=%d k=%02X pc=%04X cycles=%llu\n", halted ? 1 : 0,
                cpu.k, cpu.pc,
                static_cast<unsigned long long>(cpu.cycles));
    return 0;
}
