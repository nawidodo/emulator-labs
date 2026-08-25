#define LABSTEST_MAIN
#include <sstream>
#include <cstdio>

#include "labstest.hpp"
#include "cd_session.hpp"

namespace {
const char* kScript =
    "# tiny session\n"
    "setloc 0:2:16\n"
    "getstat\n"
    "init\n"
    "tick 1200\n"
    "pause\n"
    "tick 250\n";

void write_script(const std::string& path) {
    FILE* f = fopen(path.c_str(), "w");
    EXPECT_TRUE(f != nullptr);
    if (!f) return;
    fputs(kScript, f);
    fclose(f);
}
}  // namespace

TEST(session, deterministic_log_transcript) {
    const std::string path = "/tmp/labs_ch45_session.txt";
    write_script(path);

    // Two fresh controllers must produce byte-identical transcripts.
    cdrom::CdRomController a(nullptr), b(nullptr);
    std::ostringstream log1, log2;
    cdlc::run_script(a, path, log1);
    cdlc::run_script(b, path, log2);
    EXPECT_EQ(log1.str(), log2.str());

    EXPECT_NE(log1.str().find("t=0 int=3 resp="), std::string::npos);
    EXPECT_NE(log1.str().find("int=3"), std::string::npos);
    EXPECT_NE(log1.str().find("int=2"), std::string::npos);  // phase-2 IRQs

    // The init completion lands exactly at t=1200 and pause at +250.
    EXPECT_NE(log1.str().find("\nt=1200 int=2"), std::string::npos);
    EXPECT_NE(log1.str().find("\nt=1450 int=2"), std::string::npos);
}

TEST(session, unknown_op_rejected) {
    cdrom::CdRomController c(nullptr);
    std::ostringstream log;
    bool threw = false;
    try {
        const std::string path = "/tmp/labs_ch45_bad.txt";
        FILE* f = fopen(path.c_str(), "w");
        EXPECT_TRUE(f != nullptr);
        if (!f) return;
        fputs("frobnicate\n", f);
        fclose(f);
        cdlc::run_script(c, path, log);
    } catch (const std::exception&) {
        threw = true;
    }
    EXPECT_TRUE(threw);
}
