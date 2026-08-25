// ll_runner — headless linked-list DMA walker CLI (chapter 43).
//
//   --chain FILE      raw little-endian u32 words loaded at word 0
//   --madr HEX        walker start address (default 0)
//   --max-packets N   safety cap (default 4096)
//   --trace FILE      write walk trace lines
//   --capture FILE    write delivered GPU payload words (raw LE u32)
//   --otc N           build an OTC table of N entries instead of walking
//   --otc-base HEX    OTC start address (default 0x100)
//   --dump FILE       dump raw words (used with --otc)
//
// Exit codes: 0 ok, 2 usage/IO error, 3 cap hit on malformed chain.
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "linked_list.hpp"

namespace {

std::vector<uint32_t> load_words(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path);
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
    const size_t n = bytes.size() / 4;
    std::vector<uint32_t> words(n);
    if (n) std::memcpy(words.data(), bytes.data(), n * 4);
    return words;
}

void save_bytes(const std::string& path, const void* data, size_t len) {
    std::ofstream out(path, std::ios::binary);
    if (!len && !data) return;  // empty dump is fine
    out.write(static_cast<const char*>(data), static_cast<std::streamsize>(len));
    if (!out) throw std::runtime_error("cannot write " + path);
}

uint32_t parse_hex(const char* s) {
    return static_cast<uint32_t>(std::stoul(s, nullptr, 0));
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 1) {
        std::cout <<
            "usage: ll_runner --chain FILE [--madr HEX] [--max-packets N]\n"
            "                 [--trace FILE] [--capture FILE]\n"
            "       ll_runner --otc N [--otc-base HEX] [--dump FILE]\n";
        return 0;
    }

    std::string chain_path, trace_path, capture_path, dump_path;
    uint32_t madr = 0, otc_base = 0x100, max_packets = 4096;
    long otc_count = -1;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto need = [&](const char* what) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "missing value for " << what << "\n";
                exit(2);
            }
            return argv[++i];
        };
        if (a == "--chain") chain_path = need("--chain");
        else if (a == "--madr") madr = parse_hex(need("--madr"));
        else if (a == "--max-packets")
            max_packets = static_cast<uint32_t>(std::stoul(need("--max-packets")));
        else if (a == "--trace") trace_path = need("--trace");
        else if (a == "--capture") capture_path = need("--capture");
        else if (a == "--otc") otc_count = std::stol(need("--otc"));
        else if (a == "--otc-base") otc_base = parse_hex(need("--otc-base"));
        else if (a == "--dump") dump_path = need("--dump");
        else { std::cerr << "unknown arg " << a << "\n"; return 2; }
    }

    try {
        ps1::Ram ram;

        if (otc_count >= 0) {
            ps1::otc_build(ram, otc_base, static_cast<uint32_t>(otc_count));
            if (!dump_path.empty()) {
                // Dump the touched window: [base-count*4+4 .. base].
                std::vector<uint32_t> table;
                for (uint32_t i = 0; i < static_cast<uint32_t>(otc_count); ++i)
                    table.push_back(ram.read(otc_base - 4 * i));
                save_bytes(dump_path, table.data(), table.size() * 4);
            }
            return 0;
        }

        const auto words = load_words(chain_path);
        for (size_t i = 0; i < words.size() && i < ram.word_slots(); ++i)
            ram.write(static_cast<uint32_t>(4 * i), words[i]);

        ps1::CaptureSink gpu;
        ps1::TraceLog trace;
        const ps1::WalkResult r =
            ps1::walk_gpu_list(ram, madr, gpu, &trace, max_packets);

        if (!trace_path.empty()) {
            std::ofstream out(trace_path);
            for (const auto& line : trace) out << line << '\n';
        }
        if (!capture_path.empty())
            save_bytes(capture_path, gpu.words().data(),
                       gpu.words().size() * 4);

        if (r.cap_hit) {
            std::cerr << "cap hit after " << r.packets << " packets\n";
            return 3;
        }
        std::cout << "packets=" << r.packets << " words=" << r.words
                  << " cycles=" << r.cycles << " terminated="
                  << (r.terminated ? "yes" : "no") << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 2;
    }
}
