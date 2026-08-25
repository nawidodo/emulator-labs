// Interactive/scriptable debugger entry point.
//
//   ch06_03_repl --rom PATH [--script FILE] [--quirks SPEC]
//
// Without --script the REPL reads stdin (interactive). With --script it
// consumes the file and prints a deterministic transcript on stdout, which
// is what the graded tests diff.

#include <fstream>
#include <iostream>
#include <sstream>

#include "debugger.hpp"

namespace {

const ch06::Chip8Quirks* quirks_by_name(const std::string& name,
                                        ch06::Chip8Quirks& storage) {
    if (name == "MODERN") return &ch06::kModernQuirks;
    if (name == "COSMAC_VIP") return &ch06::kCosmacVipQuirks;
    if (name == "CHIP48") {
        storage.vf_reset = false;
        storage.shift_uses_vy = true;
        storage.load_store_leaves_i = false;
        storage.wrapping = false;
        storage.jump_bnnn_x = true;
        return &storage;
    }
    return nullptr;
}

}  // namespace

int main(int argc, char** argv) {
    std::string rom_path, script_path, quirks_spec;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if ((a == "--rom" || a == "--script" || a == "--quirks" ||
             a == "--input-file") &&
            i + 1 < argc) {
            const std::string v = argv[++i];
            if (a == "--rom") rom_path = v;
            else if (a == "--script") script_path = v;
            else if (a == "--quirks") quirks_spec = v;
            // --input-file accepted as a no-op alias so shared harnesses
            // can drive every ch06 binary uniformly.
        } else if (a == "--help" || a == "-h") {
            std::cout << "usage: ch06_03_repl --rom PATH [--script FILE] "
                         "[--quirks SPEC]\n";
            return 0;
        } else {
            std::cerr << "error: unknown argument: " << a << "\n";
            return 1;
        }
    }
    if (rom_path.empty()) {
        std::cerr << "error: --rom is required\n";
        return 1;
    }

    std::ifstream rf(rom_path, std::ios::binary);
    if (!rf) {
        std::cerr << "error: cannot open rom: " << rom_path << "\n";
        return 1;
    }
    std::vector<uint8_t> rom((std::istreambuf_iterator<char>(rf)),
                             std::istreambuf_iterator<char>());

    ch06::Chip8Quirks quirk_storage;
    const ch06::Chip8Quirks* quirks = &ch06::kModernQuirks;
    if (!quirks_spec.empty()) {
        quirks = quirks_by_name(quirks_spec, quirk_storage);
        if (!quirks) {
            std::cerr << "error: unknown quirks profile: " << quirks_spec << "\n";
            return 1;
        }
    }

    ch06::Chip8 cpu;
    cpu.reset(*quirks);
    cpu.load(rom);

    std::ifstream sf(script_path);
    if (!script_path.empty() && !sf) {
        std::cerr << "error: cannot open script: " << script_path << "\n";
        return 1;
    }
    std::istream& in = script_path.empty() ? std::cin : static_cast<std::istream&>(sf);
    ch06::Debugger dbg(cpu, in, std::cout);
    dbg.set_prog_range(uint16_t(ch06::kProgBase + rom.size()));
    dbg.run();
    return 0;
}
