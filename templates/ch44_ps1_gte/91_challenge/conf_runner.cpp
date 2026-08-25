// conf_runner — GTE conformance harness CLI (chapter 44).
//
//   --inputs FILE    snapshot records (see gte_conf.hpp header comment)
//   --outputs FILE   one result line per RUN
#include <fstream>
#include <iostream>
#include <string>

#include "gte_conf.hpp"

int main(int argc, char** argv) {
    if (argc == 1) {
        std::cout << "usage: ch44_conf_runner --inputs FILE "
                     "[--outputs FILE]\n";
        return 0;
    }
    std::string in_path, out_path;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto need = [&](const char* what) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "missing value for " << what << "\n";
                exit(2);
            }
            return argv[++i];
        };
        if (a == "--inputs") in_path = need("--inputs");
        else if (a == "--outputs") out_path = need("--outputs");
        else { std::cerr << "unknown arg " << a << "\n"; return 2; }
    }
    if (out_path.empty()) {
        const bool ok = gteconf::run_file(in_path, std::cout);
        return ok ? 0 : 2;
    }
    std::ofstream out(out_path);
    const bool ok = gteconf::run_file(in_path, out);
    if (!ok) {
        std::cerr << "error: cannot open " << in_path << "\n";
        return 2;
    }
    return 0;
}
