/**
 * @file launcher_layout.h
 * @brief Arduino-free geometry of the launcher's icon ring and the touch hit tests based on it.
 *
 * Extracted from the original launcher.cpp (_menu_init / _icon_list_init / _launcher_loop,
 * Forairaaaaa, M5Stack M5Dial-UserDemo, MIT) so it can be unit-tested on the host
 * (test/host/test_launcher_layout.cpp). Kept free of Arduino/M5 includes for that reason.
 */
#pragma once
#include <cmath>
#include <stdint.h>

/** Launcher ring layout: positions of the menu slots / icons and touch hit tests. */
namespace launcher_layout {

constexpr int CENTER_X = 120;     ///< centre of the 240x240 round panel
constexpr int CENTER_Y = 120;
constexpr int RING_SLOTS = 10;    ///< selector stops on the ring (original n = 10)
constexpr int MENU_RING_R = 60;   ///< radius of the smooth_menu item ring (selector path)
constexpr int ICON_RING_R = 95;   ///< radius of the icon ring (original 190 / 2)
constexpr int ICON_COUNT = 8;     ///< slots 0..7 carry an app icon; 8 and 9 stay empty
constexpr int ICON_HIT_R = 22;    ///< touch radius of an icon (= drawn ICON_RADIUS)
constexpr int CENTER_HIT_R = 50;  ///< touch radius of the centre "open" area

/** Ring slot of each app (the launcher factory switch and the standalone sketches use these). */
enum Slot : int {
    SLOT_LCD_TEST = 0,
    SLOT_RTC_TEST = 1,
    SLOT_RFID_TEST = 2,
    SLOT_BRIGHTNESS = 3,
    SLOT_WIFI_SCAN = 4,
    SLOT_BLE_SERVER = 5,
    SLOT_TEMP_DEMO = 6,
    SLOT_MORE_MENU = 7,
};

/** Integer screen point. */
struct Point {
    int x;
    int y;
};

/**
 * Position of ring slot `slot` at `radius`, exactly as the original computes it.
 * why 3.14 and int truncation: the original uses `a + r * std::cos(2 * 3.14 * i / n)`
 * assigned to an int; keeping it bit-identical keeps every icon on its original pixel.
 * Slot 0 is at 3 o'clock; slots advance clockwise on screen (y grows downwards).
 */
inline Point ringPoint(int slot, int radius) {
    double a = 2 * 3.14 * slot / RING_SLOTS;
    return {static_cast<int>(CENTER_X + radius * std::cos(a)),
            static_cast<int>(CENTER_Y + radius * std::sin(a))};
}

/** @return true if (x, y) is inside the centre circle (boundary included, as the original). */
inline bool inCenter(int x, int y) {
    int dx = x - CENTER_X;
    int dy = y - CENTER_Y;
    return dx * dx + dy * dy <= CENTER_HIT_R * CENTER_HIT_R;
}

/**
 * Icon under a touch point.
 * @return icon index 0..ICON_COUNT-1 whose circle (ICON_HIT_R) contains (x, y), else -1
 * why no overlap check: neighbouring icon centres are ~58 px apart (> 2 * ICON_HIT_R), so at
 * most one icon can match.
 */
inline int iconAt(int x, int y) {
    for (int i = 0; i < ICON_COUNT; ++i) {
        Point p = ringPoint(i, ICON_RING_R);
        int dx = x - p.x;
        int dy = y - p.y;
        if (dx * dx + dy * dy <= ICON_HIT_R * ICON_HIT_R) return i;
    }
    return -1;
}

/**
 * Name of the app behind slot `slot`, used in the `launcher: select` / `app: ... not ported`
 * log lines; the names match the standalone example sketches.
 * @return app name, or "-" for an empty slot / out-of-range index
 */
inline const char* appName(int slot) {
    static const char* const names[ICON_COUNT] = {"LcdTest",  "RtcTest",   "RfidTest", "Brightness",
                                                  "WifiScan", "BleServer", "TempDemo", "MoreMenu"};
    return (slot >= 0 && slot < ICON_COUNT) ? names[slot] : "-";
}

/**
 * Icon / theme colour (RGB888) of the app behind slot `slot` (original icon_color_list).
 * why here: the launcher and the standalone sketches (m5dd::initGui) must give an app the
 * same background colour; one table keeps them in step.
 * @return colour, or 0 (black) for an empty slot / out-of-range index
 */
inline uint32_t appColor(int slot) {
    static const uint32_t colors[ICON_COUNT] = {0xFD5C4C, 0x577EFF, 0x03A964, 0x1AA198,
                                                0xEB8429, 0x04A279, 0x008CD6, 0x5D7BA2};
    return (slot >= 0 && slot < ICON_COUNT) ? colors[slot] : 0;
}

}  // namespace launcher_layout
