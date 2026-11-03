/**
 * @file report.h
 * @brief Arduino-free record of the factory test's operator verdicts and its summary text.
 *
 * New in the M5DialDemo port: the original factory test (main/apps/app_factory_test,
 * Forairaaaaa, M5Stack M5Dial-UserDemo, MIT) had no verdicts - the operator judged each step
 * by eye and nothing was recorded. Kept free of Arduino/M5 includes so it can be unit-tested on
 * the host (test/host/test_report.cpp).
 */
#pragma once
#include <stdint.h>

#include <string>

/** Factory test types shared by the app, the verdict prompt and the host tests. */
namespace factory_test {

/** The factory test steps, in the order they run. */
enum class Step { Lcd, Rtc, Rfid, Io };

/** Operator verdict of one step; Pending until the step's prompt was answered. */
enum class Verdict { Pending, Pass, Fail, Skip };

constexpr int STEP_COUNT = 4;  ///< number of Step values

/** @return the step's upper-case name as logged and shown: "LCD", "RTC", "RFID", "IO". */
inline const char* stepName(Step s) {
    switch (s) {
        case Step::Lcd: return "LCD";
        case Step::Rtc: return "RTC";
        case Step::Rfid: return "RFID";
        case Step::Io: return "IO";
    }
    return "?";
}

/** @return the verdict's upper-case name: "PENDING", "PASS", "FAIL", "SKIP". */
inline const char* verdictName(Verdict v) {
    switch (v) {
        case Verdict::Pending: return "PENDING";
        case Verdict::Pass: return "PASS";
        case Verdict::Fail: return "FAIL";
        case Verdict::Skip: return "SKIP";
    }
    return "?";
}

/** The verdict of every step; all Pending when constructed. */
class FactoryReport {
   public:
    /** Records (or replaces) the verdict of a step. */
    void set(Step s, Verdict v) { _v[static_cast<int>(s)] = v; }

    /** @return the verdict of a step (Pending if not set yet). */
    Verdict get(Step s) const { return _v[static_cast<int>(s)]; }

    /**
     * @return true if every step is Pass or Skip; false if any step failed or is still
     * pending (an unanswered step never counts as a pass).
     */
    bool allPassedOrSkipped() const {
        for (Verdict v : _v)
            if (v != Verdict::Pass && v != Verdict::Skip) return false;
        return true;
    }

    /** @return the summary-screen line of one step, e.g. "IO: SKIP". */
    std::string line(Step s) const {
        return std::string(stepName(s)) + ": " + verdictName(get(s));
    }

    /**
     * @return the serial summary, all steps in order, e.g.
     * "factory: LCD=PASS RTC=PASS RFID=FAIL IO=SKIP" (log it with M5_LOGI("%s", ...)).
     */
    std::string serial() const {
        std::string out = "factory:";
        for (int i = 0; i < STEP_COUNT; ++i) {
            Step s = static_cast<Step>(i);
            out += ' ';
            out += stepName(s);
            out += '=';
            out += verdictName(get(s));
        }
        return out;
    }

   private:
    Verdict _v[STEP_COUNT] = {Verdict::Pending, Verdict::Pending, Verdict::Pending,
                              Verdict::Pending};
};

}  // namespace factory_test
