/**
 * @file host_test.h
 * @brief Minimal host unit-test harness for the Arduino-free parts of M5DialDemo
 *        (built by tools/test_host.sh with the host compiler, never by Arduino).
 *
 * Usage: `HOST_TEST(name) { HT_EQ(expected, actual); }` in any .cpp file in test/host/;
 * test_main.cpp runs every registered test and exits with the number of failed tests.
 */
#pragma once
#include <cstdio>
#include <functional>
#include <vector>

namespace host_test {

/** One registered test case. */
struct Case {
    const char* name;
    std::function<void()> fn;
};

/** All registered test cases (function-local static: safe across translation units). */
inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

/** Number of failed checks in the test currently running. */
inline int& failures() {
    static int n = 0;
    return n;
}

/** Registers a test case at static-initialisation time. */
struct Registrar {
    Registrar(const char* name, std::function<void()> fn) { registry().push_back({name, fn}); }
};

}  // namespace host_test

#define HT_CAT2(a, b) a##b
#define HT_CAT(a, b) HT_CAT2(a, b)

/** Defines and registers a test case. */
#define HOST_TEST(name)                                                          \
    static void name();                                                          \
    static host_test::Registrar HT_CAT(ht_reg_, name)(#name, name);              \
    static void name()

/** Checks that two integral values are equal; reports file:line on mismatch. */
#define HT_EQ(expected, actual)                                                  \
    do {                                                                         \
        long long e_ = (long long)(expected), a_ = (long long)(actual);          \
        if (e_ != a_) {                                                          \
            std::printf("  %s:%d: expected %s == %lld, got %lld\n", __FILE__,    \
                        __LINE__, #actual, e_, a_);                              \
            ++host_test::failures();                                             \
        }                                                                        \
    } while (0)
