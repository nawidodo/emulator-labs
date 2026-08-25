#pragma once
// Generated golden hashes for ch01/99_coding_test.
// Produced by running the chapter-03 reference generator TWICE per
// variant (byte-identical required); see tests/public/
// ch01_lab_infrastructure/provenance.md for the exact commands.
#include <cstddef>

namespace ch01_goldens {

struct Entry {
    const char* key;
    const char* path;
    const char* fnv;
};

inline constexpr Entry kEntries[] = {
    {"seven/none", "core.cpp.tpl", "FD1B08674E1ED42A"},
    {"seven/none", "manifest.json", "40398A02038FA96F"},
    {"seven/none", "meta.ini", "158184053A0CF41F"},
    {"seven/none", "util.py.tpl", "5E09D46A854183E1"},
    {"seven/1", "core.cpp.tpl", "D6BE870866CAF4B7"},
    {"seven/1", "manifest.json", "DB51D51B2BD13DBD"},
    {"seven/1", "meta.ini", "158184053A0CF41F"},
    {"seven/1", "util.py.tpl", "5E09D46A854183E1"},
    {"seven/2", "core.cpp.tpl", "D6BE870866CAF4B7"},
    {"seven/2", "manifest.json", "0EC7C5C383CA4F98"},
    {"seven/2", "meta.ini", "158184053A0CF41F"},
    {"seven/2", "util.py.tpl", "DCCF6691BA899FD0"},
    {"seven/3", "core.cpp.tpl", "F35AB857B5E33892"},
    {"seven/3", "manifest.json", "63EA0DC5D8382EB9"},
    {"seven/3", "meta.ini", "158184053A0CF41F"},
    {"seven/3", "util.py.tpl", "DCCF6691BA899FD0"},
    {"seven/4", "core.cpp.tpl", "F35AB857B5E33892"},
    {"seven/4", "manifest.json", "CD7B11D8B7598276"},
    {"seven/4", "meta.ini", "158184053A0CF41F"},
    {"seven/4", "util.py.tpl", "FC3F72F8E77BA4E7"},
    {"seven/5", "core.cpp.tpl", "1DF26CDFEE8A332A"},
    {"seven/5", "manifest.json", "98591C12C478586E"},
    {"seven/5", "meta.ini", "158184053A0CF41F"},
    {"seven/5", "util.py.tpl", "FC3F72F8E77BA4E7"},
    {"seven/6", "core.cpp.tpl", "1DF26CDFEE8A332A"},
    {"seven/6", "manifest.json", "4A131574D7A58131"},
    {"seven/6", "meta.ini", "158184053A0CF41F"},
    {"seven/6", "util.py.tpl", "462226F87D3566E8"},
    {"seven/7", "core.cpp.tpl", "CC25F60AECBD6175"},
    {"seven/7", "manifest.json", "F1041949876F9A6F"},
    {"seven/7", "meta.ini", "158184053A0CF41F"},
    {"seven/7", "util.py.tpl", "462226F87D3566E8"},
    {"seven/solution", "core.cpp.tpl", "CC25F60AECBD6175"},
    {"seven/solution", "manifest.json", "100583DF63B15B33"},
    {"seven/solution", "meta.ini", "158184053A0CF41F"},
    {"seven/solution", "util.py.tpl", "462226F87D3566E8"},
    {"challenge_a/solution", "manifest.json", "FAD758341117B472"},
    {"challenge_a/solution", "mini.hpp", "44A9C694102B2584"},
    {"challenge_b/solution", "manifest.json", "F09B21ABDFF5B557"},
    {"challenge_b/solution", "mini.hpp", "8D0223BEAFC18AE8"},
    {"challenge_c/solution", "manifest.json", "C8615C6D79A3795B"},
    {"challenge_c/solution", "mini.hpp", "2EA8FB14677D1DF4"},
};

}  // namespace ch01_goldens
