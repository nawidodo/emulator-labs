#pragma once
#include <cstdint>
#include <istream>
#include <map>
#include <string>
#include <vector>

// Chapter 8 coding test: given two execution traces of the same program,
// report the FIRST DIVERGENCE (curriculum §54) — the earliest line where
// the traces disagree — instead of staring at the final symptom.
//
// Canonical trace format (AUTHORING.md): whitespace-separated key=value
// tokens per line, e.g.
//   pc=0005 op=CD af=0102 bc=0000 de=0000 hl=0000 sp=2000 cyc=17

namespace labsdiv {

using Fields = std::map<std::string, std::string>;

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Split one line into key=value tokens on whitespace. Lines without any
// '=' token are invalid -> false.
inline bool parse_line(const std::string& line, Fields& out) {
    out.clear();
    std::istringstream in(line);
    std::string tok;
    while (in >> tok) {
        const auto eq = tok.find('=');
        if (eq == std::string::npos) return false;
        out[tok.substr(0, eq)] = tok.substr(eq + 1);
    }
    return !out.empty();
}
//@LABS-STUB
// TODO(1): split line into key=value tokens on whitespace; reject tokens
// without '='; return false when nothing parsed.
inline bool parse_line(const std::string&, Fields&) {
    return false;  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Parse every non-blank line of a trace stream. Blank lines are skipped
// so trailing newlines don't shift alignment between tools.
inline std::vector<Fields> parse_trace(std::istream& in) {
    std::vector<Fields> rows;
    std::string line;
    while (std::getline(in, line)) {
        if (line.find_first_not_of(" \t\r\n") == std::string::npos)
            continue;
        Fields f;
        if (parse_line(line, f)) rows.push_back(f);
    }
    return rows;
}
//@LABS-STUB
// TODO(2): read all non-blank lines through parse_line into rows.
inline std::vector<Fields> parse_trace(std::istream&) {
    return {};  // wrong on purpose
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// Two rows are equal iff they carry exactly the same key=value set.
inline bool rows_equal(const Fields& a, const Fields& b) {
    if (a.size() != b.size()) return false;
    auto ia = a.begin();
    auto ib = b.begin();
    for (; ia != a.end(); ++ia, ++ib) {
        if (ia->first != ib->first || ia->second != ib->second)
            return false;
    }
    return true;
}
//@LABS-STUB
// TODO(3): compare both keys and values of the two rows.
inline bool rows_equal(const Fields&, const Fields&) {
    return true;  // wrong on purpose: hides every divergence
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// First divergence: 1-based row index of the earliest mismatched row.
// If one trace is a strict prefix of the other, the first missing row IS
// the divergence. Identical traces yield 0.
inline int first_divergence(const std::vector<Fields>& golden,
                            const std::vector<Fields>& actual) {
    const size_t n = golden.size() < actual.size() ? golden.size()
                                                   : actual.size();
    for (size_t i = 0; i < n; ++i) {
        if (!rows_equal(golden[i], actual[i]))
            return static_cast<int>(i) + 1;
    }
    if (golden.size() != actual.size())
        return static_cast<int>(n) + 1;
    return 0;
}
//@LABS-STUB
// TODO(4): scan aligned rows and return the 1-based index of the first
// mismatch; length mismatch counts at the first extra row; identical -> 0.
inline int first_divergence(const std::vector<Fields>&,
                            const std::vector<Fields>&) {
    return -1;  // wrong on purpose
}
//@LABS-END

}  // namespace labsdiv
