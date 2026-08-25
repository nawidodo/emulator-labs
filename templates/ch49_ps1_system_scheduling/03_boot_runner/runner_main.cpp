// Headless boot runner for the ch49 stand-in PS1 SoC (curriculum §52 CLI
// shape). Wires mini core + GPU/DMA/CD/SPU/INTC behind one event scheduler
// and runs a boot image until HALT or a cycle budget.
//
//   ch49_03_boot_runner_runner --rom prog.bin --headless \
//       --cycles 40000 [--frames N] [--trace t.log] [--hash-frame ev.log] \
//       [--input-file script.txt]
//
// ROM format: raw little-endian words loaded into RAM at word 0; entry is
// word 0. See tests/public/ch49_ps1_system_scheduling/roms/ for committed
// fixtures, each with an .asm.txt listing and provenance note.
//
// The EVENT LOG (--hash-frame output) is one line per milestone/event:
//
//   cyc=0 evt=boot
//   cyc=695 evt=gpu_cmd pixels=1024 cmd=00100400
//   cyc=695 evt=gpu_idle
//   cyc=N evt=latch line=2 src=cd        (IRQ latches, in dispatch order)
//   cyc=N evt=milestone val=<n>          (program writes 1F801FF0)
//   cyc=N evt=dma_done words=<n>
//   cyc=N evt=halt
//
// hashed as FNV-1a 64 over the raw bytes. Instruction traces go to
// --trace in the canonical shape: pc=<hex8> op=<hex8> cyc=<n>.
//
// The optional input file is a deterministic device script applied at
// cycle 0 after boot scheduling, one command per line ('#' comments):
//
//   MASK <hex>     INTC mask write
//   DMA <words>    kick the DMA channel with <words> words
//   GPUCMD <hex>   queue one GP0 command word
//   CDREAD         start a sector read
//   SPUON          enable the sample-period interrupt
#include <cstdint>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../01_scheduler_core/scheduler.hpp"
#include "../02_mini_devices/system.hpp"
#include "../shared/fnv.hpp"

namespace {

constexpr uint64_t kDefaultBudgetCycles = 200000;

uint64_t fnv_of_string(const std::vector<std::string>& lines) {
    std::string blob;
    for (const auto& l : lines) {
        blob += l;
        blob += '\n';
    }
    return labshash::fnv64(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(blob.data()), blob.size()));
}

void write_lines(const std::string& path,
                 const std::vector<std::string>& lines) {
    std::ofstream f(path, std::ios::binary);
    for (const auto& l : lines) f << l << "\n";
}

bool load_rom_file(ps1sys::System<sched::Scheduler>& sys,
                   const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::vector<char> bytes((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
    sys.load_rom(reinterpret_cast<const uint8_t*>(bytes.data()),
                 bytes.size());
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    std::string rom, script, hash_out, trace_out;
    long long cycles_arg = -1;
    long frames_arg = -1;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--rom") rom = next();
        else if (a == "--input-file") script = next();
        else if (a == "--hash-frame") hash_out = next();
        else if (a == "--trace") trace_out = next();
        else if (a == "--cycles") cycles_arg = std::stoll(next());
        else if (a == "--frames") frames_arg = std::stol(next());
        else if (a == "--headless") { /* accepted no-op */ }
        else if (a == "--help") {
            std::cout << "usage: ch49_03_boot_runner_runner [--rom FILE]"
                      " [--input-file SCRIPT] [--cycles N] [--frames N]"
                      " [--trace FILE] [--hash-frame FILE] --headless\n";
            return 0;
        } else {
            std::cerr << "unknown arg: " << a << "\n";
            return 2;
        }
    }

    sched::Scheduler sch;
    ps1sys::System<sched::Scheduler> sys(sch);
    sys.reset();

    if (!rom.empty() && !load_rom_file(sys, rom)) {
        std::cerr << "cannot load rom: " << rom << "\n";
        return 2;
    }
    // Scripts apply AFTER reset so their cycle-0 MMIO pokes land behind
    // the boot events in insertion order (and thus in tie-break order).
    if (!script.empty()) {
        std::ifstream s(script);
        if (!s) {
            std::cerr << "cannot open script: " << script << "\n";
            return 2;
        }
        if (!sys.apply_script(s)) {
            std::cerr << "bad script command in: " << script << "\n";
            return 2;
        }
    }

    uint64_t budget = kDefaultBudgetCycles;
    if (cycles_arg >= 0) budget = static_cast<uint64_t>(cycles_arg);
    if (frames_arg >= 0)
        budget = std::max<uint64_t>(
            budget, static_cast<uint64_t>(frames_arg) *
                        ps1sys::kVideoFrameCycles);
    sys.run_until(budget);

    const auto& log = sys.event_log();
    const uint64_t digest = fnv_of_string(log);
    if (!trace_out.empty()) write_lines(trace_out, sys.trace());
    if (!hash_out.empty()) write_lines(hash_out, log);

    std::printf("events=%zu cyc=%llu halted=%d fnv64=%016llX\n",
                log.size(), static_cast<unsigned long long>(sys.now()),
                sys.halted() ? 1 : 0, static_cast<unsigned long long>(digest));
    return 0;
}
