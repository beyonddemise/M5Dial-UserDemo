/**
 * @file test_main.cpp
 * @brief Runs every HOST_TEST registered by the other .cpp files in test/host/.
 *        Prints one PASS/FAIL line per test; exit code = number of failed tests (0 = all pass).
 */
#include "host_test.h"

/** Runs all registered tests and reports a summary. */
int main() {
    int failed = 0;
    for (const auto& c : host_test::registry()) {
        host_test::failures() = 0;
        c.fn();
        bool ok = host_test::failures() == 0;
        std::printf("%s %s\n", ok ? "PASS" : "FAIL", c.name);
        if (!ok) ++failed;
    }
    std::printf("%d tests, %d failed\n", (int)host_test::registry().size(), failed);
    return failed;
}
