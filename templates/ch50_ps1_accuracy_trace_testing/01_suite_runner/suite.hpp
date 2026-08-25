#pragma once
#ifdef _WIN32
#  include <process.h>   // _spawnvp / _cwait (MSVC)
#else
#  include <sys/wait.h>  // POSIX: fork / exec / waitpid
#  include <unistd.h>
#endif

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "checks.hpp"

// Suite manifest support.
//
// A suite file is plain text, one case per line:
//
//   # comment
//   case cpu_trace        builtin.cpu_trace
//   case seed01           bin build/skels/ch50_ps1_accuracy_trace_testing/90_debug/ch50_90_regress_tests seed01
//
// Grammar: `case <name> <binary-or-builtin> [args...]`
//   * `<binary-or-builtin>` starting with `builtin.` names a built-in check
//     from checks.hpp; anything else is an executable path, relative to the
//     runner's working directory, followed by optional argv words.
//   * Blank lines and `#` comments are ignored.
//
// The runner prints one PASS/FAIL line per case plus a summary line and
// exits 0 iff every case passed — that exit code is what makes a suite
// usable from ctest, make accuracy and the hidden grader alike.
namespace psxsuite {

struct Case {
    std::string name;
    bool is_builtin = false;
    std::string builtin;             // valid when is_builtin
    std::string binary;              // valid when !is_builtin
    std::vector<std::string> args;   // valid when !is_builtin
    bool ok = false;                 // well-formed line?
    std::string error;               // why not well-formed
};

struct Result {
    std::string name;
    bool pass = false;
    std::string reason;  // empty on pass
};

//@LABS-BEGIN 1
// Parse one suite line (already stripped of leading/trailing whitespace).
// On success fills `c` and sets ok=true. Malformed lines set ok=false and a
// human-readable `error`; they must never throw.
//@LABS-SOLUTION
inline void parse_case(const std::string& line, Case& c) {
    c = Case{};
    std::vector<std::string> tok;
    size_t i = 0;
    while (i <= line.size()) {
        size_t j = line.find_first_of(" \t", i);
        if (j == std::string::npos) j = line.size();
        if (j > i) tok.push_back(line.substr(i, j - i));
        i = j + 1;
    }
    if (tok.size() < 3 || tok[0] != "case") {
        c.error = "expected: case <name> <binary-or-builtin> [args...]";
        return;
    }
    c.name = tok[1];
    if (tok[2].rfind("builtin.", 0) == 0) {
        if (tok.size() != 3) {
            c.error = "builtin cases take no arguments";
            return;
        }
        c.is_builtin = true;
        c.builtin = tok[2];
    } else {
        c.binary = tok[2];
        c.args.assign(tok.begin() + 3, tok.end());
    }
    c.ok = true;
}
//@LABS-STUB
inline void parse_case(const std::string& line, Case& c) {
    // TODO(1): parse the manifest grammar documented above. The stub marks
    // every line malformed, so any suite reports zero runnable cases.
    c = Case{};
    c.error = "TODO(1): parse_case unimplemented";
}
//@LABS-END

// Split a whole manifest into Cases, dropping blanks and '#' comments.
inline std::vector<Case> parse_suite(const std::string& text) {
    std::vector<Case> out;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t eol = text.find('\n', pos);
        if (eol == std::string::npos) eol = text.size();
        std::string line = text.substr(pos, eol - pos);
        pos = eol + 1;
        const size_t first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos || line[first] == '#') continue;
        const size_t last = line.find_last_not_of(" \t\r");
        Case c;
        parse_case(line.substr(first, last - first + 1), c);
        out.push_back(std::move(c));
    }
    return out;
}

// Run one external case: returns the child's exit code, or -1 when it could
// not be spawned at all (missing binary etc.). POSIX-only by design — the
// course targets the same unix-y environments as tools/labs.
inline int run_bin(const std::string& path,
                   const std::vector<std::string>& args) {
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(path.c_str()));
    for (const auto& a : args)
        argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

#ifdef _WIN32
    // MSVC: _spawnvp with _P_WAIT returns the child exit code directly.
    const intptr_t rc = ::_spawnvp(_P_WAIT, path.c_str(), argv.data());
    return rc == -1 ? -1 : static_cast<int>(rc);
#else
    const pid_t pid = ::fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        ::execvp(argv[0], argv.data());
        _exit(127);  // exec failed — binary missing / not executable
    }
    int status = 0;
    ::waitpid(pid, &status, 0);
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
#endif
}

// Execute one parsed case, appending exactly one Result.
inline void run_case(const Case& c, Result& r) {
    r = Result{c.name, false, {}};
    if (!c.ok) {
        r.reason = c.error.empty() ? "malformed case" : c.error;
        return;
    }
    if (c.is_builtin) {
        const BuiltinCheck* b = find_builtin(c.builtin);
        if (!b) {
            r.reason = "unknown builtin: " + c.builtin;
            return;
        }
        std::string why;
        r.pass = b->fn(&why);
        if (!r.pass) r.reason = why.empty() ? "check failed" : why;
        return;
    }
    const int rc = run_bin(c.binary, c.args);
    if (rc < 0) {
        r.reason = "could not spawn: " + c.binary;
        return;
    }
    if (rc != 0) {
        r.reason = "exit " + std::to_string(rc) + ": " + c.binary;
        return;
    }
    r.pass = true;
}

// Run a whole suite in order.
inline std::vector<Result> run_suite(const std::vector<Case>& cases) {
    std::vector<Result> out;
    out.reserve(cases.size());
    for (const auto& c : cases) {
        Result r;
        run_case(c, r);
        out.push_back(std::move(r));
    }
    return out;
}

//@LABS-BEGIN 2
// Format the report: one "PASS <name>" / "FAIL <name>: <reason>" line per
// result (in order), then the summary line "== <passed>/<total> checks
// passed". The exact byte layout matters: 91_challenge pins its FNV-64 hash.
//@LABS-SOLUTION
inline std::string format_report(const std::vector<Result>& rs) {
    std::string out;
    unsigned passed = 0;
    for (const auto& r : rs) {
        out += r.pass ? "PASS " : "FAIL ";
        out += r.name;
        if (!r.pass) {
            out += ": ";
            out += r.reason;
        }
        out += '\n';
        passed += r.pass ? 1u : 0u;
    }
    out += "== " + std::to_string(passed) + "/" + std::to_string(rs.size()) +
           " checks passed\n";
    return out;
}
//@LABS-STUB
inline std::string format_report(const std::vector<Result>& rs) {
    // TODO(2): emit the PASS/FAIL lines plus the "== p/t checks passed"
    // summary. The stub produces an empty report, which both the runner's
    // output test and the challenge hash pin will catch.
    (void)rs;
    return "";
}
//@LABS-END

}  // namespace psxsuite
