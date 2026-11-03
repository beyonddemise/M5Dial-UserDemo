/**
 * @file mem_soak.h
 * @brief Arduino-free sequencing of the MemSoak device test (examples/MemSoak): which app runs,
 *        when it is done, the timeouts, the heap-leak check, and the success-only verdict.
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
/// free internal heap a cycle may end below the first cycle's before the soak calls it a leak
/// (why not 0: allocator fragmentation and lazily freed task memory move the reading by
/// a few hundred bytes; a real leak per Wi-Fi + BLE open/close is far larger)
constexpr uint32_t LEAK_TOLERANCE = 4096;

/** Which app the soak runs now, or how it ended. */
enum class Phase : uint8_t {
    Wifi,    ///< WiFi_Scan runs until its scan completed
    Ble,     ///< BLE_Server runs for its fixed time
    Heap,    ///< a cycle ended; waiting for the driver's free-heap reading (cycleHeap())
    Done,    ///< every cycle succeeded (the only state that may log `soak: done`)
    Failed,  ///< stopped; failReason() says why
};

/** What the driver does after a poll. */
enum class Step : uint8_t {
    Run,       ///< keep running the current app (M5Dial.update(); onRunning())
    Switch,    ///< destroy the current app, then create the app of phase()
    CycleEnd,  ///< destroy the current app, read and log the heap, pass it to cycleHeap()
               ///< and act on the step that returns
    Done,      ///< every cycle passed, also the heap check: log `soak: done`, stop
    Fail,      ///< destroy the current app (if any), log `soak: FAIL <failReason()>`, stop
    Idle,      ///< the soak is already over; nothing to do
};

/**
 * The soak's cycle state machine: Wifi -> Ble -> Heap -> Wifi ... for `cycles` rounds.
 *
 * Usage: create the app of phase(), run its onSetup()/onCreate(), call appStarted(millis()),
 * then each frame poll() and act on the Step; after Step::CycleEnd, destroy the app, read the
 * free heap and act on cycleHeap()'s Step. A failure is sticky, and Done is only reached when
 * every cycle passed and no cycle lost heap against the first, so `soak: done` is a
 * success-only signal.
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
    /** @return why the soak failed (`wifi timeout`, `wifi quit`, `ble quit`, `heap leak`, or
     *          fail()'s), "" if not */
    const char* failReason() const { return _reason; }
    /** @return the first cycle's free heap (the leak baseline), 0 before it was read */
    uint32_t heapBaseline() const { return _baseline; }

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
                    _phase = Phase::Heap;
                    return Step::CycleEnd;
                }
                if (appQuit) return fail("ble quit");
                return Step::Run;
            case Phase::Heap:
                return Step::CycleEnd;  // still waiting for cycleHeap()
            case Phase::Done:
            case Phase::Failed:
                break;
        }
        return Step::Idle;
    }

    /**
     * Judges the free heap read after a cycle's apps were destroyed (Phase::Heap only).
     * why the first cycle is the baseline, not the heap before it: the first Wi-Fi and BLE
     * init allocate once (NVS, PHY calibration, NimBLE pools), which is not a leak.
     * @param freeBytes free internal heap (heap_caps_get_free_size(MALLOC_CAP_INTERNAL))
     * @return Fail (`heap leak`) if it is more than LEAK_TOLERANCE below the baseline; else
     *         Done after the last cycle, Switch (to the next cycle's Wi-Fi) before it; Run if
     *         not in Phase::Heap (nothing changes); Idle once finished()
     */
    Step cycleHeap(uint32_t freeBytes) {
        if (finished()) return Step::Idle;
        if (_phase != Phase::Heap) return Step::Run;
        if (_done == 1) _baseline = freeBytes;
        // why this form: `freeBytes < _baseline - LEAK_TOLERANCE` underflows for a low baseline
        if (freeBytes + LEAK_TOLERANCE < _baseline) return fail("heap leak");
        if (_done >= _cycles) {
            _phase = Phase::Done;
            return Step::Done;
        }
        _phase = Phase::Wifi;
        return Step::Switch;
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
    uint32_t _baseline = 0;
    const char* _reason = "";
};

}  // namespace mem_soak
