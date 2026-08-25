// ch39 / 99_coding_test runner — executes the UNSEEN exception scenario
// (two timed delay-slot interrupt preemptions + one syscall skip) headless
// and emits the standard trace/state-digest pair.
//
//   ch39_99_coding_test_runner [--cycles N] [--trace FILE]
//                              [--hash-frame FILE] [--headless]
//                              [--rom FILE (accepted; scenario is built-in)]
//
// The scenario program is assembled inline by scenario.hpp — there is no
// external fixture, because the coding test must stay unseen.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "coding_test.hpp"
#include "scenario.hpp"
#include "../91_challenge/fnv.hpp"

namespace {

using psx::r3000a::NestedCpu;

const char* exc_name(psx::r3000a::ExcCode c) {
    switch (c) {
        case psx::r3000a::ExcCode::Interrupt: return "int";
        case psx::r3000a::ExcCode::Syscall: return "syscall";
        default: return "exc";
    }
}

uint64_t state_digest(const NestedCpu& cpu, long cycles) {
    std::vector<uint8_t> blob;
    blob.insert(blob.end(), cpu.bus.scratchpad.begin(),
                cpu.bus.scratchpad.end());
    for (uint32_t r : cpu.gpr) {
        for (int i = 0; i < 4; ++i) blob.push_back((r >> (8 * i)) & 0xFF);
    }
    const uint32_t words[5] = {cpu.pc, cpu.cop0.sr, cpu.cop0.cause,
                               cpu.cop0.epc, static_cast<uint32_t>(cycles)};
    for (uint32_t w : words) {
        for (int i = 0; i < 4; ++i) blob.push_back((w >> (8 * i)) & 0xFF);
    }
    return psx::r3000a::fnv64(blob);
}

void usage(FILE* out) {
    std::fprintf(out,
                 "usage: ch39_99_coding_test_runner [--cycles N]\n"
                 "       [--trace FILE] [--hash-frame FILE] [--headless]\n"
                 "       [--rom FILE]   (accepted for CLI parity; the\n"
                 "                       scenario is built into the binary)\n");
}

}  // namespace

int main(int argc, char** argv) {
    const char* trace_path = nullptr;
    const char* hash_path = nullptr;
    long cycles = 96;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--help") {
            usage(stdout);
            return 0;
        } else if (a == "--trace" && i + 1 < argc) {
            trace_path = argv[++i];
        } else if (a == "--hash-frame" && i + 1 < argc) {
            hash_path = argv[++i];
        } else if ((a == "--cycles" || a == "--frames") && i + 1 < argc) {
            cycles = std::strtol(argv[++i], nullptr, 0);
        } else if (a == "--headless") {
            // no-op for CLI parity
        } else if (a == "--rom") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "--rom requires a path\n");
                return 2;
            }
            ++i;  // accepted; the coding-test scenario is built in
        } else {
            std::fprintf(stderr, "unknown or incomplete flag: %s\n",
                         a.c_str());
            usage(stderr);
            return 2;
        }
    }

    NestedCpu cpu;
    cpu.reset();
    cpu.bus.load_bios(psx::r3000a::build_scenario_image());
    cpu.irq_cycle_a = psx::r3000a::kIrqCycleFirstBalSlot;
    cpu.irq_cycle_b = psx::r3000a::kIrqCycleSecondBalSlot;

    FILE* trace = trace_path ? std::fopen(trace_path, "w") : nullptr;
    if (trace_path && !trace) {
        std::fprintf(stderr, "cannot write trace: %s\n", trace_path);
        return 2;
    }

    for (long cyc = 1; cyc <= cycles; ++cyc) {
        const uint32_t at = cpu.pc;
        const bool slot = cpu.will_execute_delay_slot();
        uint32_t word = 0;
        bus_read(&cpu.bus, at, &word);
        psx::r3000a::StepEvent ev = cpu.step_irq();
        // An interrupt delivery consumes the whole cycle (no instruction
        // retires), so a trapped Interrupt raised while a delay slot was
        // pending came from the fake controller.
        const bool from_irq =
            ev.trapped && ev.code == psx::r3000a::ExcCode::Interrupt && slot;
        if (!trace) continue;
        if (ev.trapped) {
            if (from_irq) {
                std::fprintf(trace,
                             "pc=%08x op=%08x irq=1 exc=%s bd=%d epc=%08x "
                             "vec=%08x cyc=%ld\n",
                             at, word, exc_name(ev.code), ev.bd ? 1 : 0,
                             ev.epc, ev.vector, cyc);
            } else {
                std::fprintf(trace,
                             "pc=%08x op=%08x exc=%s bd=%d epc=%08x "
                             "vec=%08x cyc=%ld\n",
                             at, word, exc_name(ev.code), ev.bd ? 1 : 0,
                             ev.epc, ev.vector, cyc);
            }
        } else {
            std::fprintf(trace, "pc=%08x op=%08x cyc=%ld\n", at, word, cyc);
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
                         state_digest(cpu, cycles)));
        std::fclose(h);
    }
    return 0;
}
