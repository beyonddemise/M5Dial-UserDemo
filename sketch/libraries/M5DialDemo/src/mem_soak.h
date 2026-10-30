/**
 * @file mem_soak.h
 * @brief Arduino-free sequencing of the MemSoak device test (examples/MemSoak): which app runs,
 *        when it is done, the timeouts, and the success-only verdict.
 *
 * Kept free of Arduino/M5 includes so it is unit-tested on the host
 * (test/host/test_mem_soak.cpp); the sketch owns the apps, the clock and the log lines.
 */
#pragma once
#include <stdint.h>

/** MemSoak: Wi-Fi and BLE opened and closed in turn to catch heap leaks (Review Focus 5). */
namespace mem_soak {

constexpr int CYCLES = 3;                  ///< Wi-Fi + BLE rounds per soak
constexpr uint32_t WIFI_TIMEOUT_MS = 20000;  ///< longest wait for the scan to complete
constexpr uint32_t BLE_RUN_MS = 5000;      ///< how long the BLE server runs per cycle

/** Which app the soak runs now, or how it ended. */
enum class Phase : uint8_t {
    Wifi,    ///< WiFi_Scan runs until its scan completed
    Ble,     ///< BLE_Server runs for its fixed time
    Done,    ///< every cycle succeeded (the only state that may log `soak: done`)
    Failed,  ///< stopped; failReason() says why
};

/** What the driver does after a poll. */
enum class Step : uint8_t {
    Run,       ///< keep running the current app (M5Dial.update(); onRunning())
    Switch,    ///< destroy the current app, then create the app of phase()
    CycleEnd,  ///< destroy the current app, log the heap; then `soak: done` if phase() is Done,
               ///< else create the app of phase()
    Fail,      ///< destroy the current app (if any), log `soak: FAIL <failReason()>`, stop
    Idle,      ///< the soak is already over; nothing to do
};

/**
 * The soak's cycle state machine: Wifi -> Ble -> (cycle end) -> Wifi ... for `cycles` rounds.
 *
 * Usage: create the app of phase(), run its onSetup()/onCreate(), call appStarted(millis()),
 * then each frame poll() and act on the Step. A failure is sticky and Done is only reached
 * when every cycle passed, so `soak: done` is a success-only signal.
 */
class Sequencer {
   public:
    /**
     * @param cycles Wi-Fi + BLE rounds (> 0)
     * @param wifiTimeoutMs fail if the scan has not completed after this long
     * @param bleRunMs run time of the BLE app per cycle
     */
    explicit Sequencer(int cycles = CYCLES, uint32_t wifiTimeoutMs = WIFI_TIMEOUT_MS,
                       uint32_t bleRunMs = BLE_RUN_MS)
        : _cycles(cycles), _wifiTimeout(wifiTimeoutMs), _bleRun(bleRunMs) {}

    /** @return the app to run now, or the final state */
    Phase phase() const { return _phase; }
    /** @return configured number of cycles */
    int cycles() const { return _cycles; }
    /** @return completed cycles (each logged a heap line) */
    int cyclesDone() const { return _done; }
    /** @return true once Done or Failed */
    bool finished() const { return _phase == Phase::Done || _phase == Phase::Failed; }
    /** @return true only if every cycle completed */
    bool succeeded() const { return _phase == Phase::Done; }
    /** @return why the soak failed (`wifi timeout`, `wifi quit`, `ble quit`, or fail()'s), "" if not */
    const char* failReason() const { return _reason; }

    /**
     * Starts the current app's clock. Call after its onCreate().
     * why not at the switch: NimBLE init inside onCreate() takes a while and must not count
     * against the app's run time or timeout.
     * @param now millis()
     */
    void appStarted(uint32_t now) { _start = now; }

    /**
     * Decides what happens after one frame of the current app.
     * @param now millis()
     * @param appDone the app's done condition (Wi-Fi: WiFi_Scan::scanDone(); ignored for BLE,
     *                which runs for bleRunMs)
     * @param appQuit the app asked to close or be destroyed itself (e.g. a button click)
     * @return the step to take; Idle once finished()
     */
    Step poll(uint32_t now, bool appDone, bool appQuit) {
        // why unsigned subtraction: stays correct across the 49-day millis() wrap
        uint32_t elapsed = now - _start;
        switch (_phase) {
            case Phase::Wifi:
                // why done first: a scan that completed counts even if the click or the
                // timeout landed in the same frame - the soak destroys the app either way
                if (appDone) {
                    _phase = Phase::Ble;
                    return Step::Switch;
                }
                if (appQuit) return fail("wifi quit");
                if (elapsed >= _wifiTimeout) return fail("wifi timeout");
                return Step::Run;
            case Phase::Ble:
                if (elapsed >= _bleRun) {
                    ++_done;
                    _phase = (_done >= _cycles) ? Phase::Done : Phase::Wifi;
                    return Step::CycleEnd;
                }
                if (appQuit) return fail("ble quit");
                return Step::Run;
            case Phase::Done:
            case Phase::Failed:
                break;
        }
        return Step::Idle;
    }

    /**
     * Fails the soak for a reason found by the driver (e.g. an app could not be allocated).
     * @param reason static string, logged after `soak: FAIL `
     * @return Fail, or Idle if the soak had already finished (first reason / success kept)
     */
    Step fail(const char* reason) {
        if (finished()) return Step::Idle;
        _phase = Phase::Failed;
        _reason = reason;
        return Step::Fail;
    }

   private:
    int _cycles;
    uint32_t _wifiTimeout;
    uint32_t _bleRun;
    Phase _phase = Phase::Wifi;
    int _done = 0;
    uint32_t _start = 0;
    const char* _reason = "";
};

}  // namespace mem_soak
