// Headless runner for ch04_chip8_cpu (curriculum §52 CLI contract).
//
//   <runner> --rom FILE [--cycles N] [--frames N] [--trace FILE]
//             [--regs FILE] [--input-file FILE] [--quirks PROFILE] --headless
//
// Trace line format (consumed by tools/labs/compare_trace.py), emitted AFTER
// each instruction with post-state:
//   pc=0200 op=00EE V0=00 .. VF=01 I=000 SP=00 DT=00 cyc=11
//
// Input file contract (ch04 CPU stage): one line per executed instruction.
// Each line lists pressed key nibbles ("14A") or "." for none; lines run out
// -> no keys pressed. Deterministic by construction.
#include <cstdint>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "chip8.hpp"

namespace {

std::string hex2(unsigned v) {
    char b[8];
    std::snprintf(b, sizeof(b), "%02X", v & 0xFFu);
    return b;
}
std::string hex3(unsigned v) {
    char b[8];
    std::snprintf(b, sizeof(b), "%03X", v & 0x0FFFu);
    return b;
}
std::string hex4(unsigned v) {
    char b[8];
    std::snprintf(b, sizeof(b), "%04X", v);
    return b;
}

struct Options {
    const char* rom = nullptr;
    long cycles = 1000;
    long frames = -1;              // accepted per CLI contract; alias for cycles here
    const char* trace_path = nullptr;
    const char* regs_path = nullptr;
    const char* input_path = nullptr;
    const char* quirks_profile = "cosmac";
};

void usage(std::FILE* out, const char* argv0) {
    std::fprintf(out,
                 "usage: %s --rom FILE [--cycles N] [--frames N] [--trace FILE]\n"
                 "          [--regs FILE] [--input-file FILE] [--quirks cosmac|modern]\n"
                 "          [--headless]\n",
                 argv0);
}

std::vector<std::string> load_key_script(const char* path) {
    std::vector<std::string> lines;
    if (!path) return lines;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

void apply_keys(chip8::Chip8& cpu, const std::string& line) {
    for (bool& k : cpu.key) k = false;
    if (line.empty() || line == ".") return;
    for (char c : line) {
        int n = c >= '0' && c <= '9' ? c - '0'
              : c >= 'a' && c <= 'f' ? c - 'a' + 10
              : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (n >= 0) cpu.key[size_t(n)] = true;
    }
}

void apply_quirks(chip8::Chip8& cpu, const std::string& profile) {
    // cosmac: classic defaults already set by reset(); modern flips the
    // historically divergent bits to their CHIP-48/HIP-8 values.
    if (profile == "modern") {
        cpu.quirks.shift_uses_vy = true;
        cpu.quirks.load_store_leaves_i = true;
        cpu.quirks.vf_reset = true;
        cpu.quirks.wrapping = false;
    } else if (profile != "cosmac") {
        std::fprintf(stderr, "unknown quirks profile '%s' (cosmac|modern)\n",
                     profile.c_str());
    }
}

std::string trace_line(const chip8::Chip8& cpu, uint16_t op) {
    char buf[256];
    int n = std::snprintf(buf, sizeof(buf), "pc=%03X op=%04X", cpu.pc, op);
    for (int r = 0; r < 16; ++r)
        n += std::snprintf(buf + n, sizeof(buf) - size_t(n), " V%d=%02X", r,
                           cpu.v[r]);
    std::snprintf(buf + n, sizeof(buf) - size_t(n),
                  " I=%03X SP=%02X DT=%02X cyc=%llu", cpu.idx, cpu.sp, cpu.dtimer,
                  static_cast<unsigned long long>(cpu.cycles));
    return buf;
}

std::string regs_json(const chip8::Chip8& cpu) {
    std::ostringstream os;
    os << "{\"cycles\":" << cpu.cycles << ",\"dt\":" << unsigned(cpu.dtimer)
       << ",\"halted\":" << (cpu.halted ? "true" : "false")
       << ",\"illegal_op\":\"" << hex4(cpu.illegal_op) << "\""
       << ",\"i\":\"" << hex3(cpu.idx) << "\",\"pc\":\"" << hex3(cpu.pc) << "\""
       << ",\"quirks\":{\"load_store_leaves_i\":"
       << (cpu.quirks.load_store_leaves_i ? "true" : "false")
       << ",\"shift_uses_vy\":" << (cpu.quirks.shift_uses_vy ? "true" : "false")
       << ",\"vf_reset\":" << (cpu.quirks.vf_reset ? "true" : "false")
       << ",\"wrapping\":" << (cpu.quirks.wrapping ? "true" : "false") << "}"
       << ",\"sound\":" << unsigned(cpu.stimer)
       << ",\"sp\":" << unsigned(cpu.sp) << ",\"stack\":[";
    for (int s = 0; s < chip8::Chip8::kStackSlots; ++s)
        os << (s ? "," : "") << cpu.stack[s];
    os << "],\"v\":[";
    for (int r = 0; r < 16; ++r)
        os << (r ? "," : "") << "\"" << hex2(cpu.v[r]) << "\"";
    os << "]}\n";
    return os.str();
}

}  // namespace

int main(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&](const char* what) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "missing value for %s\n", what);
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--help") { usage(stdout, argv[0]); return 0; }
        else if (a == "--rom") opt.rom = next("--rom");
        else if (a == "--cycles") opt.cycles = std::strtol(next("--cycles"), nullptr, 0);
        else if (a == "--frames") opt.frames = std:: strtol(next("--frames"), nullptr, 0);
        else if (a == "--trace") opt.trace_path = next("--trace");
        else if (a == "--regs") opt.regs_path = next("--regs");
        else if (a == "--input-file") opt.input_path = next("--input-file");
        else if (a == "--quirks") opt.quirks_profile = next("--quirks");
        else if (a == "--headless") { /* implied: no window exists in ch04 */ }
        else if (a == "--hash-frame") {
            // No framebuffer exists until ch05; accept the flag for CLI
            // uniformity and say so instead of writing a bogus hash file.
            std::fprintf(stderr, "note: ch04 CPU stage has no framebuffer; "
                                 "--hash-frame ignored\n");
            (void)next("--hash-frame");
        } else { std::fprintf(stderr, "unknown arg '%s'\n", a.c_str()); usage(stderr, argv[0]); return 2; }
    }
    if (!opt.rom) { usage(stderr, argv[0]); return 2; }

    std::ifstream rom_in(opt.rom, std::ios::binary);
    if (!rom_in) { std::fprintf(stderr, "cannot open rom '%s'\n", opt.rom); return 2; }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(rom_in)),
                               std::istreambuf_iterator<char>());

    chip8::Chip8 cpu;
    cpu.reset();
    apply_quirks(cpu, opt.quirks_profile);
    cpu.load(bytes);

    const auto keys = load_key_script(opt.input_path);
    std::ofstream trace_out;
    if (opt.trace_path) trace_out.open(opt.trace_path, std::ios::binary);

    const long budget = opt.frames >= 0 ? opt.frames : opt.cycles;
    for (long k = 0; k < budget; ++k) {
        if (k < static_cast<long>(keys.size()))
            apply_keys(cpu, keys[size_t(k)]);
        const uint16_t op = cpu.step();
        if (opt.trace_path) trace_out << trace_line(cpu, op) << '\n';
        if (cpu.halted) {
            if (opt.trace_path)
                trace_out << "# halt illegal_op=" << hex4(cpu.illegal_op)
                          << " pc=" << hex3(cpu.pc) << " cyc=" << cpu.cycles << '\n';
            break;
        }
    }

    if (opt.regs_path) {
        std::ofstream out(opt.regs_path, std::ios::binary);
        out << regs_json(cpu);
    }
    return 0;
}
