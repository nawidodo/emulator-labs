// ch01/02_hex_dumper — CLI front-end for the dump() core.
//
//   ch01_02_hex_dumper [--file PATH | PATH] [--output PATH] | --help
//
// Reads the input file as raw bytes, writes the hex dump to --output
// (or stdout when omitted). Exit 0 on success, 1 on usage or IO errors.

#include <cstdio>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "dumper.hpp"

namespace {

int print_usage(std::FILE* out) {
    std::fputs("usage: ch01_02_hex_dumper [--file PATH | PATH] "
               "[--output PATH]\n"
               "       ch01_02_hex_dumper --help\n"
               "Hex-dumps PATH (16 bytes per row, offset column, ASCII "
               "gutter) to\n--output or stdout. Empty input prints only the "
               "final offset line.\n",
               out);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string file_arg;
    std::string out_arg;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--help") {
            return print_usage(stdout);
        }
        if (a == "--file" || a == "--output") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: %s needs a value\n", a.c_str());
                return 1;
            }
            (a == "--file" ? file_arg : out_arg) = argv[++i];
        } else if (!a.empty() && a[0] == '-') {
            std::fprintf(stderr, "error: unknown option '%s'\n", a.c_str());
            print_usage(stderr);
            return 1;
        } else {
            file_arg = a;  // positional path
        }
    }

    if (file_arg.empty()) {
        std::fprintf(stderr, "error: no input file (use --file or positional "
                             "PATH)\n");
        print_usage(stderr);
        return 1;
    }

    std::ifstream in(file_arg, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "error: cannot open '%s'\n", file_arg.c_str());
        return 1;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>{});

    std::ofstream out_file;
    std::ostream* os = &std::cout;
    if (!out_arg.empty()) {
        out_file.open(out_arg, std::ios::binary);
        if (!out_file) {
            std::fprintf(stderr, "error: cannot open '%s' for writing\n",
                         out_arg.c_str());
            return 1;
        }
        os = &out_file;
    }

    ch01::dump(std::span<const uint8_t>(data), *os);
    return 0;
}
