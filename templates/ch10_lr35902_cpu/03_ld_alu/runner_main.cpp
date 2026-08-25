// Headless SM83 runner (curriculum §52 shape).
//
//   ch10_03_ld_alu_runner --rom prog.bin --headless --cycles 10000 \
//       --trace trace.log --dump state.txt
// Trace line per executed instruction, emitted AFTER it completes:
//   pc=<instr addr> op=<first opcode byte> af/bc/de/hl/sp=<regs> cyc=<total>
//
// In this CPU-only phase `--frames` and `--input-file` are accepted and
// ignored (no PPU/joypad yet); `--hash-frame` writes the canonical final
// CPU-state dump so golden hashes stay meaningful without a framebuffer.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "bus.hpp"
#include "cpu.hpp"

namespace {

struct Options {
    const char* rom = nullptr;
    long cycles = 1000000;
    const char* trace = nullptr;
    const char* dump = nullptr;
    const char* hash_frame = nullptr;
};

std::vector<uint8_t> read_file(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", path);
        std::exit(2);
    }
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    if (size > 0) std::fread(data.data(), 1, data.size(), f);
    std::fclose(f);
    return data;
}

void write_file(const char* path, const std::string& text) {
    FILE* f = std::fopen(path, "wb");
    if (!f) {
        std::fprintf(stderr, "cannot write %s\n", path);
        std::exit(2);
    }
    std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
}

std::string state_dump(const gb::Cpu& cpu) {
    char line[128];
    std::snprintf(line, sizeof(line),
                  "af=%04X bc=%04X de=%04X hl=%04X sp=%04X pc=%04X "
                  "cyc=%llu halted=%d trap=%d\n",
                  cpu.af(), cpu.bc(), cpu.de(), cpu.hl(), cpu.sp, cpu.pc,
                  static_cast<unsigned long long>(cpu.cyc),
                  cpu.halted ? 1 : 0, cpu.trap ? 1 : 0);
    return line;
}

}  // namespace

int main(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> const char* { return argv[++i]; };
        if (a == "--rom") opt.rom = next();
        else if (a == "--cycles") opt.cycles = std::atol(next());
        else if (a == "--trace") opt.trace = next();
        else if (a == "--dump") opt.dump = next();
        else if (a == "--hash-frame") opt.hash_frame = next();
        else if (a == "--headless" || a == "--frames" || a == "--input-file") {
            // accepted: no PPU or input devices in this chapter
            if (a == "--frames" || a == "--input-file") (void)next();
        } else if (a == "--help" || a == "-h") {
            std::printf(
                "usage: %s --rom PATH [--cycles N] [--trace FILE] "
                "[--dump FILE] [--hash-frame FILE] --headless\n",
                argv[0]);
            return 0;
        } else {
            std::fprintf(stderr, "unknown flag: %s\n", a.c_str());
            return 2;
        }
    }
    if (!opt.rom) {
        std::fprintf(stderr, "missing --rom\n");
        return 2;
    }

    gb::FlatBus bus;
    bus.load(read_file(opt.rom));

    gb::Cpu cpu;
    cpu.bus = &bus;

    std::string trace;
    while (!cpu.halted && !cpu.trap && cpu.cyc < static_cast<uint64_t>(opt.cycles)) {
        const uint16_t instr_pc = cpu.pc;
        const uint8_t op = bus.read(instr_pc);
        cpu.step();
        char line[160];
        std::snprintf(line, sizeof(line),
                      "pc=%04X op=%02X af=%04X bc=%04X de=%04X hl=%04X "
                      "sp=%04X cyc=%llu\n",
                      instr_pc, op, cpu.af(), cpu.bc(), cpu.de(), cpu.hl(),
                      cpu.sp, static_cast<unsigned long long>(cpu.cyc));
        trace += line;
    }

    if (opt.trace) write_file(opt.trace, trace);
    if (opt.dump || opt.hash_frame) {
        const std::string dump = state_dump(cpu);
        if (opt.dump) write_file(opt.dump, dump);
        if (opt.hash_frame) write_file(opt.hash_frame, dump);
    }

    if (cpu.trap) {
        std::fprintf(stderr, "TRAP: unimplemented opcode %02X at %04X after %llu cycles\n",
                     bus.read(cpu.pc), cpu.pc,
                     static_cast<unsigned long long>(cpu.cyc));
        return 1;
    }
    std::printf("%s", state_dump(cpu).c_str());
    return 0;
}
