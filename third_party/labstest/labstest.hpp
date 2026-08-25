// labstest.hpp — minimal deterministic C++20 test framework for emulator-labs.
//
//   #define LABSTEST_MAIN
//   #include "labstest.hpp"
//
//   TEST(alu, add_sets_carry) {
//       EXPECT_EQ(carry_of(0xFF, 1), 1);
//   }
//
// Zero dependencies, no network fetches, stable across platforms so golden
// hashes stay meaningful.
#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace labstest {

struct TestCase {
    const char* suite;
    const char* name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

inline int& failures() {
    static int f = 0;
    return f;
}

struct Registrar {
    Registrar(const char* s, const char* n, std::function<void()> f) {
        registry().push_back({s, n, std::move(f)});
    }
};

template <typename T>
std::string repr(const T& v) {
    if constexpr (std::is_same_v<T, bool>) {
        return v ? "true" : "false";
    } else if constexpr (std::is_arithmetic_v<T>) {
        return std::to_string(v);
    } else {
        return "<expr>";
    }
}

template <typename A, typename B>
void check_eq(bool ok, const char* file, int line, const char* expr,
              const A& a, const B& b) {
    if (!ok) {
        ++failures();
        std::printf("  FAIL %s:%d: %s (lhs=%s rhs=%s)\n", file, line, expr,
                    repr(a).c_str(), repr(b).c_str());
    }
}

inline void run_all(std::string_view filter = {}) {
    int ran = 0;
    for (const auto& tc : registry()) {
        std::string full = std::string(tc.suite) + "." + tc.name;
        if (!filter.empty() && full.find(filter) == std::string::npos) continue;
        int before = failures();
        std::printf("[ RUN  ] %s\n", full.c_str());
        tc.fn();
        if (failures() == before)
            std::printf("[  OK  ] %s\n", full.c_str());
        else
            std::printf("[ FAIL ] %s\n", full.c_str());
        ++ran;
    }
    std::printf("== %d tests, %d failed assertions ==\n", ran, failures());
}

}  // namespace labstest

#define LABSTEST_CONCAT_(a, b) a##b
#define LABSTEST_CONCAT(a, b) LABSTEST_CONCAT_(a, b)

#define TEST(suite, name)                                                     \
    static void suite##_##name();                                             \
    static ::labstest::Registrar LABSTEST_CONCAT(                             \
        labs_reg_, __LINE__)(#suite, #name, &suite##_##name);                 \
    static void suite##_##name()

#define EXPECT_TRUE(expr)                                                     \
    ::labstest::check_eq(static_cast<bool>(expr), __FILE__, __LINE__, #expr,  \
                         true, true)
#define EXPECT_FALSE(expr) EXPECT_TRUE(!(expr))
#define EXPECT_EQ(a, b)                                                       \
    do {                                                                      \
        /* Decay-COPY both sides: binds-by-value survive temporaries       */ \
        /* (*f().member would dangle under const auto&) and are still      */ \
        /* evaluated exactly once.                                         */ \
        const auto labs_a_ = (a);                                             \
        const auto labs_b_ = (b);                                             \
        ::labstest::check_eq(labs_a_ == labs_b_, __FILE__, __LINE__,          \
                             #a " == " #b, labs_a_, labs_b_);                 \
    } while (0)
#define EXPECT_NE(a, b)                                                       \
    do {                                                                      \
        const auto labs_a_ = (a);                                             \
        const auto labs_b_ = (b);                                             \
        ::labstest::check_eq(labs_a_ != labs_b_, __FILE__, __LINE__,          \
                             #a " != " #b, labs_a_, labs_b_);                 \
    } while (0)

#ifdef LABSTEST_MAIN
int main(int argc, char** argv) {
    std::string filter = argc > 1 ? argv[1] : "";
    ::labstest::run_all(filter);
    return ::labstest::failures() == 0 ? 0 : 1;
}
#endif
