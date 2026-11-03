/**
 * @file test_report.cpp
 * @brief FactoryReport: the factory test's per-step operator verdicts, the summary lines on
 *        the screen (`IO: SKIP`) and the serial summary line
 *        (`factory: LCD=PASS RTC=PASS RFID=FAIL IO=SKIP`).
 */
#include <cstring>

#include "apps/factory_test/report.h"
#include "host_test.h"

using factory_test::FactoryReport;
using factory_test::Step;
using factory_test::Verdict;

HOST_TEST(report_starts_all_pending) {
    FactoryReport r;
    HT_EQ((int)Verdict::Pending, (int)r.get(Step::Lcd));
    HT_EQ((int)Verdict::Pending, (int)r.get(Step::Rtc));
    HT_EQ((int)Verdict::Pending, (int)r.get(Step::Rfid));
    HT_EQ((int)Verdict::Pending, (int)r.get(Step::Io));
    HT_EQ(0, std::strcmp("factory: LCD=PENDING RTC=PENDING RFID=PENDING IO=PENDING",
                         r.serial().c_str()));
}

HOST_TEST(report_set_get_per_step) {
    FactoryReport r;
    r.set(Step::Rfid, Verdict::Fail);
    HT_EQ((int)Verdict::Fail, (int)r.get(Step::Rfid));
    HT_EQ((int)Verdict::Pending, (int)r.get(Step::Lcd));
    r.set(Step::Rfid, Verdict::Pass);  // a later verdict replaces the earlier one
    HT_EQ((int)Verdict::Pass, (int)r.get(Step::Rfid));
}

HOST_TEST(report_line_exact_text) {
    FactoryReport r;
    r.set(Step::Io, Verdict::Skip);
    r.set(Step::Lcd, Verdict::Pass);
    r.set(Step::Rfid, Verdict::Fail);
    HT_EQ(0, std::strcmp("IO: SKIP", r.line(Step::Io).c_str()));
    HT_EQ(0, std::strcmp("LCD: PASS", r.line(Step::Lcd).c_str()));
    HT_EQ(0, std::strcmp("RFID: FAIL", r.line(Step::Rfid).c_str()));
    HT_EQ(0, std::strcmp("RTC: PENDING", r.line(Step::Rtc).c_str()));
}

HOST_TEST(report_serial_exact_text) {
    FactoryReport r;
    r.set(Step::Lcd, Verdict::Pass);
    r.set(Step::Rtc, Verdict::Pass);
    r.set(Step::Rfid, Verdict::Fail);
    r.set(Step::Io, Verdict::Skip);
    HT_EQ(0, std::strcmp("factory: LCD=PASS RTC=PASS RFID=FAIL IO=SKIP", r.serial().c_str()));
}

HOST_TEST(report_all_passed_or_skipped) {
    FactoryReport r;
    HT_EQ(false, r.allPassedOrSkipped());  // all pending
    r.set(Step::Lcd, Verdict::Pass);
    r.set(Step::Rtc, Verdict::Skip);
    r.set(Step::Rfid, Verdict::Pass);
    HT_EQ(false, r.allPassedOrSkipped());  // IO still pending
    r.set(Step::Io, Verdict::Skip);
    HT_EQ(true, r.allPassedOrSkipped());
    r.set(Step::Rtc, Verdict::Fail);
    HT_EQ(false, r.allPassedOrSkipped());  // any fail
}

HOST_TEST(report_step_and_verdict_names) {
    HT_EQ(0, std::strcmp("LCD", factory_test::stepName(Step::Lcd)));
    HT_EQ(0, std::strcmp("RTC", factory_test::stepName(Step::Rtc)));
    HT_EQ(0, std::strcmp("RFID", factory_test::stepName(Step::Rfid)));
    HT_EQ(0, std::strcmp("IO", factory_test::stepName(Step::Io)));
    HT_EQ(0, std::strcmp("PENDING", factory_test::verdictName(Verdict::Pending)));
    HT_EQ(0, std::strcmp("PASS", factory_test::verdictName(Verdict::Pass)));
    HT_EQ(0, std::strcmp("FAIL", factory_test::verdictName(Verdict::Fail)));
    HT_EQ(0, std::strcmp("SKIP", factory_test::verdictName(Verdict::Skip)));
}
