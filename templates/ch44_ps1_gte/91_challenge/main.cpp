#define LABSTEST_MAIN
#include <sstream>
#include <cstdio>

#include "labstest.hpp"
#include "gte_conf.hpp"

TEST(conf, runs_records_and_emits_lines) {
    // Write a tiny conformance file to a fixed temp path under /tmp.
    const char* path = "/tmp/labs_ch44_conf_case.txt";
    {
        FILE* f = fopen(path, "w");
        EXPECT_TRUE(f != nullptr);
        if (!f) return;
        fputs("REG=0:00001000 REG=1:00001000 CREG=26:00000200\n"
              "CREG=24:00000400 CREG=25:00000300\n"
              "OP=RTPS SF=1 LM=0 RUN\n",
              f);
        fclose(f);
    }
    std::ostringstream out;
    EXPECT_TRUE(gteconf::run_file(path, out));
    EXPECT_TRUE(out.str().rfind("out mac0=", 0) == 0);
    EXPECT_NE(out.str().find("flag=80028002"), std::string::npos);
}
