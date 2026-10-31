/**
 * @file app_wifi_scan.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2023-08-04
 *
 * @copyright Copyright (c) 2023
 *
 * M5DialDemo port: the blocking WIFI_COMMON_TEST::SCAN::scan() (NVS + netif + esp_wifi set up
 * and torn down per scan) replaced by an asynchronous WiFi.h scan polled in onRunning(); list
 * text, cap and scroll maths in wifi_list.h (host-tested); a touch rescans; M5Dial.update() in
 * the button and touch waits; M5_LOGI log lines (docs/PORTING_NOTES.md, "Wi-Fi scan").
 */
#include "app_wifi_scan.h"

#include <M5Dial.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

#include "../../dial_encoder.h"
#include "../../m5dd_begin.h"
#include "wifi_list.h"


namespace {
constexpr int32_t ANIM_MS = 600;           ///< original overshoot time of the list
constexpr int32_t ANIM_START_Y = 100;      ///< original: the list slides in from 100 px down
constexpr uint32_t TOUCH_POLL_MS = 5;      ///< original `while (isTouched()) delay(5)` pattern
}  // namespace


using namespace MOONCAKE::USER_APP;


void WiFi_Scan::_start_scan()
{
    _gui.renderScanning();
    M5_LOGI("wifi: scan");

    // why async: the original blocked onCreate() for the whole scan (~2-4 s), so the button
    // could not quit meanwhile; scanComplete() is polled from onRunning() instead
    // why show_hidden (upstream's NULL scan config left it off): lists hidden APs as "(hidden)"
    int16_t started = WiFi.scanNetworks(/*async*/ true, /*show_hidden*/ true);
    _data.scanning = (started == WIFI_SCAN_RUNNING);
    if (!_data.scanning)
    {
        M5_LOGW("wifi: scan failed");
        _data.wifi_scan_result = "Scan failed\n";
        // why: a touch rescan may follow a scroll; the stale offset could place the short
        // failure text above the viewport
        _show_list();
    }
}


void WiFi_Scan::_poll_scan()
{
    int16_t result = WiFi.scanComplete();
    if (result == WIFI_SCAN_RUNNING)
        return;
    _data.scanning = false;

    if (result < 0)
    {
        // why: WIFI_SCAN_FAILED also covers the core's scan timeout
        M5_LOGW("wifi: scan failed");
        _data.wifi_scan_result = "Scan failed\n";
    }
    else
    {
        // why the count first (upstream fix d49e20e): it bounds the SSID(i)/RSSI(i) reads; the
        // core's scan-done handler already read the AP count before the records
        M5_LOGI("wifi: ap_count=%d", result);
        int listed = wifi_list::listedCount(result);
        _data.wifi_scan_result.clear();
        for (int i = 0; i < listed; i++)
        {
            String ssid = WiFi.SSID(i);
            int32_t rssi = WiFi.RSSI(i);
            M5_LOGI("wifi: %d %s", (int)rssi, wifi_list::sanitizeSsid(ssid.c_str()).c_str());
            _data.wifi_scan_result += wifi_list::formatLine(i, rssi, ssid.c_str());
        }
    }

    // why now, not in onDestroy only: the records (ap_count x ~80 B) are no longer needed once
    // the text is built; no PSRAM, and the 115 KB canvas is resident
    WiFi.scanDelete();

    _show_list();
}


void WiFi_Scan::_show_list()
{
    // why: knob turns made while "Scanning..." was shown would otherwise land as one jump
    _detent.reset(dialEncoder.read());

    /* List slides in from below, as on open upstream */
    _data.menu_y_offset = 0;
    _data.menu_y_offset_anim.setAnim(LVGL::overshoot, ANIM_START_Y, 0, ANIM_MS);
    _data.menu_y_offset_anim.resetTime(millis());
}


void WiFi_Scan::onSetup()
{
    setAppName("WiFi_Scan");
    setAllowBgRunning(false);

    /* Copy default value */
    WIFI_SCAN::Data_t default_data;
    _data = default_data;

    _data.ctx = static_cast<AppContext*>(getUserData());
}


/* Life cycle */
void WiFi_Scan::onCreate()
{
    M5_LOGI("app: open %s", getAppName().c_str());
    _detent.reset(dialEncoder.read());

    // why station mode, never connect: a scan needs no credentials (no secrets.h)
    WiFi.mode(WIFI_STA);
    _start_scan();
}


void WiFi_Scan::onRunning()
{
    /* If button pressed: hold until button release, then bye */
    if (m5dd::buttonClicked())
    {
        destroyApp();
        return;
    }

    if (_data.scanning)
    {
        _poll_scan();
        return;
    }

    /* If scrolled */
    int32_t steps = m5dd::readDetentsWithClick(_detent);
    if (steps != 0)
    {
        uint32_t now = millis();
        _data.menu_y_offset = wifi_list::scrollOffset(_data.menu_y_offset, steps,
                                                      now - _data.scroll_speed_time_count);

        /* Reset anim */
        _data.menu_y_offset_anim.setAnim(LVGL::overshoot, _data.menu_y_offset_anim.getValue(now),
                                         _data.menu_y_offset, ANIM_MS);
        _data.menu_y_offset_anim.resetTime(now);

        /* Reset time count */
        _data.scroll_speed_time_count = now;
    }

    /* If touched: rescan once the finger is lifted (not upstream, see PORTING_NOTES) */
    if (M5Dial.Touch.getDetail().isPressed())
    {
        // why M5Dial.update(): the touch state only changes inside update(); a button press
        // made and released entirely inside this wait is consumed here (no quit)
        while (M5Dial.Touch.getDetail().isPressed())
        {
            delay(TOUCH_POLL_MS);
            M5Dial.update();
        }
        _start_scan();
        return;
    }

    /* Update page */
    _gui.renderPage(_data.menu_y_offset_anim.getValue(millis()), _data.wifi_scan_result);
}


void WiFi_Scan::onDestroy()
{
    // why both: scanDelete() frees results of a scan that finished after the last poll;
    // WIFI_OFF stops the driver (aborting a running scan) and deinits it, returning its heap
    WiFi.scanDelete();
    WiFi.mode(WIFI_OFF);
    M5_LOGI("wifi: heap=%u", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    M5_LOGI("app: close %s", getAppName().c_str());
}
