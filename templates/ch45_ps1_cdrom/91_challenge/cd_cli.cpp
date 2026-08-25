// cd_cli — scripted CD-ROM session runner CLI (chapter 45).
//
//   --cue FILE --bin FILE --script FILE [--log OUT]
#include <iostream>
#include <string>

#include "cd_session.hpp"

int main(int argc, char** argv) {
    if (argc == 1) {
        std::cout << "usage: cd_cli --cue FILE --bin FILE --script FILE "
                     "[--log OUT]\n";
        return 0;
    }
    std::string cue, bin, script, log_path;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto need = [&](const char* what) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "missing value for " << what << "\n";
                exit(2);
            }
            return argv[++i];
        };
        if (a == "--cue") cue = need("--cue");
        else if (a == "--bin") bin = need("--bin");
        else if (a == "--script") script = need("--script");
        else if (a == "--log") log_path = need("--log");
        else { std::cerr << "unknown arg " << a << "\n"; return 2; }
    }
    try {
        cdrom::DiscImage disc;
        std::string err;
        if (!disc.load(cue, bin, &err)) {
            std::cerr << "error: " << err << "\n";
            return 2;
        }
        cdrom::CdRomController c(&disc);
        if (log_path.empty()) {
            cdlc::run_script(c, script, std::cout);
        } else {
            std::ofstream out(log_path);
            cdlc::run_script(c, script, out);
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 2;
    }
}
