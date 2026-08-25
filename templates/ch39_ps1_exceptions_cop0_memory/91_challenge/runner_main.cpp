// ch39 / 91_boot_mini runner — headless execution of the synthetic BIOS stub.
//
//   ch39_91_challenge_runner --rom bios_stub.bin [--cycles N] [--trace FILE]
//                            [--hash-frame FILE] [--frames N] [--headless]
//
// Trace format (tools/labs/compare_trace.py compatible), one line per
// executed instruction:
//   pc=<hex> op=<hex> cyc=<n>
// plus, when that instruction trapped into the exception model:
//   exc=<name> bd=<0|1> epc=<hex> vec=<hex>
//
// --hash-frame writes `fnv64=<16 hex>\n` over the full observable end state:
// scratchpad (1024 bytes) + all GPRs + pc + SR/CAUSE/EPC + cycle count.

#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#include "boot_mini.hpp"
#include "fnv.hpp"

namespace {

using psx::r3000a::BootMini;
using psx::r3000a::ExcCode;
using psx::r3000a::fnv64;

const char* exc_name(ExcCode c) {
    switch (c) {
        case ExcCode::Interrupt: return "int";
        case ExcCode::AddressErrorLoad: return "adel";
        case ExcCode::AddressErrorStore: return "ades";
        case ExcCode::BusErrorInstruction: return "ibe";
        case ExcCode::BusErrorData: return "dbe";
        case ExcCode::Syscall: return "syscall";
        case ExcCode::Breakpoint: return "bp";
        case ExcCode::ReservedInstruction: return "ri";
        case ExcCode::CoprocessorUnusable: return "cpu";
        default: return "exc";
    }
}

std::vector<uint8_t> load_rom(const char* path) {
    std::vector<uint8_t> bytes;
    FILE* f = std::fopen(path, "rb");
    if (!f) return bytes;
    uint8_t buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
        bytes.insert(bytes.end(), buf, buf + n);
    std::fclose(f);
    return bytes;
}

uint64_t state_digest(const BootMini& cpu, uint64_t cycles) {
    // Everything the chapter claims to observe goes into one deterministic
    // byte stream: scratchpad contents, registers, COP0 resume state, cycles.
    std::vector<uint8_t> blob;
    blob.reserve(1024 + 32 * 4 + 4 * 4 + 8);
    blob.insert(blob.end(), cpu.bus.scratchpad.begin(), cpu.bus.scratchpad.end());
    for (uint32_t r : cpu.gpr) {
        for (int i = 0; i < 4; ++i) blob.push_back((r >> (8 * i)) & 0xFF);
    }
    const uint32_t words[5] = {cpu.pc, cpu.cop0.sr, cpu.cop0.cause,
                               cpu.cop0.epc, static_cast<uint32_t>(cycles)};
    for (uint32_t w : words) {
        for (int i = 0; i < 4; ++i) blob.push_back((w >> (8 * i)) & 0xFF);
    }
    return fnv64(blob);
}

void usage(FILE* out) {
    std::fprintf(out,
                 "usage: ch39_91_challenge_runner --rom FILE [--cycles N]\n"
                 "       [--frames N] [--trace FILE] [--hash-frame FILE]\n"
                 "       [--headless] [--input-file FILE]\n");
}

}  // namespace

int main(int argc, char** argv) {
    const char* rom_path = nullptr;
    const char* trace_path = nullptr;
    const char* hash_path = nullptr;
    long cycles = 48;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--help") {
            usage(stdout);
            return 0;
        } else if (a == "--rom" && i + 1 < argc) {
            rom_path = argv[++i];
        } else if (a == "--trace" && i + 1 < argc) {
            trace_path = argv[++i];
        } else if (a == "--hash-frame" && i + 1 < argc) {
            hash_path = argv[++i];
        } else if ((a == "--cycles" || a == "--frames") && i + 1 < argc) {
            cycles = std::strtol(argv[++i], nullptr, 0);
        } else if (a == "--headless" || a == "--input-file") {
            // Accepted for CLI parity across chapters; no-op here (the CPU
            // chapter has no controller input and never renders frames).
            if (a == "--input-file") ++i;
        } else {
            std::fprintf(stderr, "unknown or incomplete flag: %s\n", a.c_str());
            usage(stderr);
            return 2;
        }
    }
    if (!rom_path) {
        std::fprintf(stderr, "--rom is required\n");
        usage(stderr);
        return 2;
    }

    const std::vector<uint8_t> rom = load_rom(rom_path);
    if (rom.empty()) {
        std::fprintf(stderr, "cannot read ROM image: %s\n", rom_path);
        return 2;
    }

    BootMini cpu;
    cpu.reset();
    cpu.bus.load_bios(rom);

    FILE* trace = trace_path ? std::fopen(trace_path, "w") : nullptr;
    if (trace_path && !trace) {
        std::fprintf(stderr, "cannot write trace: %s\n", trace_path);
        return 2;
    }

    for (long cyc = 1; cyc <= cycles; ++cyc) {
        const uint32_t at = cpu.pc;
        uint32_t word = 0;
        bus_read(&cpu.bus, at, &word);  // op column even when the fetch faults
        psx::r3000a::StepEvent ev = cpu.step();
        if (trace) {
            if (ev.trapped) {
                std::fprintf(trace,
                             "pc=%08x op=%08x exc=%s bd=%d epc=%08x "
                             "vec=%08x cyc=%ld\n",
                             at, word, exc_name(ev.code), ev.bd ? 1 : 0,
                             ev.epc, ev.vector, cyc);
            } else {
                std::fprintf(trace, "pc=%08x op=%08x cyc=%ld\n", at, word,
                             cyc);
            }
        }
    }
    if (trace) std::fclose(trace);

    if (hash_path) {
        FILE* h = std::fopen(hash_path, "w");
        if (!h) {
            std::fprintf(stderr, "cannot write hash file: %s\n", hash_path);
            return 2;
        }
        std::fprintf(h, "fnv64=%016llX\n",
                     static_cast<unsigned long long>(
                         state_digest(cpu, static_cast<uint64_t>(cycles))));
        std::fclose(h);
    }
    return 0;
}
