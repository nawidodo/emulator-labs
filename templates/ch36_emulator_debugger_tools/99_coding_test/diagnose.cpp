// ch36 coding test: diagnose a hanging ROM using only debugger tools.
//   ch36_99_diagnose --rom PATH --result FILE [--help]
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "cpudebug.hpp"
#include "fx8.hpp"

namespace {

int visit_cap = 3;      // visits that qualify as a loop
int max_steps = 32;

}  // namespace

int main(int argc, char** argv) {
    const char* rom_path = nullptr;
    const char* result_path = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--help")) {
            std::printf(
                "usage: ch36_99_diagnose --rom PATH --result FILE\n"
                "\nRuns the fixed diagnostic protocol and writes a\n"
                "diagnosis token derived from live observations.\n");
            return 0;
        }
        if (!std::strcmp(argv[i], "--rom") && i + 1 < argc) {
            rom_path = argv[++i];
        } else if (!std::strcmp(argv[i], "--result") && i + 1 < argc) {
            result_path = argv[++i];
        }
    }
    if (!rom_path || !result_path) {
        std::fprintf(stderr, "error: --rom and --result required (--help)\n");
        return 2;
    }

    fx8::Cpu cpu;
    cpu.reset();
    {
        std::ifstream in(rom_path, std::ios::binary);
        if (!in) {
            std::fprintf(stderr, "error: cannot open rom '%s'\n", rom_path);
            return 2;
        }
        std::vector<uint8_t> rom((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());
        cpu.load(rom);
    }

    dbg::Fx8Debug dbg(cpu);

    char line[128];
//@LABS-BEGIN 1
//@LABS-SOLUTION
    int visits[256] = {};
    bool loop_found = false;
    uint8_t loop_pc = 0;
    for (int s = 0; s < max_steps && !cpu.halted; ++s) {
        dbg.step();
        const uint8_t pc_after = cpu.pc;
        if (++visits[pc_after] >= visit_cap) {
            loop_pc = pc_after;
            loop_found = true;
            break;
        }
    }

    if (loop_found) {
        std::snprintf(line, sizeof(line),
                      "diagnosis=infinite_loop loop_pc=%02X insn=\"%s\" "
                      "mem20=%02X",
                      loop_pc, dbg.disasm(loop_pc).c_str(),
                      dbg.read_mem(0x20));
    } else {
        std::snprintf(line, sizeof(line), "diagnosis=clean");
    }
//@LABS-STUB
    // TODO(1): run up to max_steps instructions via dbg.step(), counting
    // post-step PC visits. The first pc reaching visit_cap(3) is
    // loop_pc — stop there and format:
    //   diagnosis=infinite_loop loop_pc=<hex> insn="<disasm>" mem20=<hex>
    // or "diagnosis=clean" when no loop / halted within budget.
    std::snprintf(line, sizeof(line), "diagnosis=unimplemented");
//@LABS-END

    std::ofstream out(result_path, std::ios::binary);
    out << line << "\n";
    if (std::strstr(line, "unimplemented") != nullptr) return 2;
    std::printf("%s\n", line);
    return 0;
}
