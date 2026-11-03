/**
 * @file more_menu_items.h
 * @brief Arduino-free parts of the More menu: its entries, row geometry, the curved x offset
 *        and text shade of the render callback, and the selector's rounded-rect mask.
 *
 * Extracted from the original app_more_menu.cpp (_create_menu) and
 * more_menu_render_callback.hpp (Forairaaaaa, M5Stack M5Dial-UserDemo, MIT) so it can be
 * unit-tested on the host (test/host/test_more_menu_items.cpp). Kept free of Arduino/M5
 * includes for that reason.
 */
#pragma once
#include <cmath>
#include <cstring>

/** Entries and drawing maths of the More menu. */
namespace more_menu_items {

constexpr int ITEM_QUIT = 0;       ///< back to the launcher
constexpr int ITEM_POWER_OFF = 1;  ///< RTC IRQ off, then M5Dial.Power.powerOff()
constexpr int ITEM_COUNT = 13;     ///< upstream 16 minus the three LVGL demos

constexpr int TEXT_W = 12;    ///< upstream text_width: px per character (efontCN_24)
constexpr int TEXT_H = 24;    ///< upstream text_height
constexpr int ITEM_X = 24;    ///< upstream x of every row
constexpr int ITEM_Y0 = 10;   ///< upstream y of the first row
constexpr int ROW_GAP = 2;    ///< upstream (text_height + 2) row pitch

/**
 * Tag of entry `i`.
 * why the LVGL entries are gone: LVGL lives only in the standalone LvglDemo sketch (D3);
 * the filler rows ("???" ... "List") stay, they only made the list long enough to scroll.
 * @return the tag, or "" for an out-of-range index
 */
inline const char* tag(int i) {
    static const char* const tags[ITEM_COUNT] = {
        "Quit", "Power Off", "???",    ":):):):)", "This", "Is",  "A",
        "Really", "Really",  "Really", "Really",   "Long", "List"};
    return (i >= 0 && i < ITEM_COUNT) ? tags[i] : "";
}

/** Integer rectangle. */
struct Rect {
    int x;
    int y;
    int w;
    int h;
};

/** @return the smooth_menu item rectangle of entry `i`, as upstream _create_menu() adds it */
inline Rect itemRect(int i) {
    return {ITEM_X, ITEM_Y0 + (TEXT_H + ROW_GAP) * i,
            TEXT_W * static_cast<int>(std::strlen(tag(i))), TEXT_H};
}

/**
 * Upstream's fast sine approximation (more_menu_render_callback.hpp), kept bit-identical so
 * the rows curve exactly as on the original.
 */
inline float approxSin(float x) {
    float t = x * 0.15915f;
    t = t - (int)t;
    if (t < 0.5)
        return (-16.0f * t * t) + (8.0f * t);
    else
        return (16.0f * t * t) - (16.0f * t) - (8.0f * t) + 8.0f;
}

/**
 * Extra x of a row `dy` px below (positive) or above the selector: 0 on the selected row,
 * growing towards the edges so the list follows the round panel.
 * why int truncation: upstream passed the float to drawString(int32_t ...).
 */
inline int curveOffsetX(float dy) {
    return static_cast<int>(120 * approxSin((dy / 120) + (3 * 3.14159f / 2)) + 120);
}

/**
 * Grey level (0..255) of a row's text `dy` px from the selector: 255 - |dy| * 2.
 * why the clamp: upstream assigned the result to uint8_t, which is undefined behaviour once
 * it is negative (|dy| > 127.5); far rows are simply black here.
 */
inline int textShade(float dy) {
    float v = 255 - std::fabs(dy) * 2;
    return v <= 0 ? 0 : static_cast<int>(v);
}

/**
 * Is pixel (px, py) inside the w x h rectangle at (x, y) with corner radius r (clamped to
 * half the smaller side)? Tested at the pixel centre, no anti-aliasing.
 */
inline bool inRoundRect(int px, int py, int x, int y, int w, int h, int r) {
    if (w <= 0 || h <= 0) return false;
    if (px < x || py < y || px >= x + w || py >= y + h) return false;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    // why doubled coordinates: pixel centres are at +0.5; doubling keeps it in integers
    bool left = px < x + r, right = px >= x + w - r;
    bool top = py < y + r, bottom = py >= y + h - r;
    if (!(left || right) || !(top || bottom)) return true;  // not in a corner square
    int cx = left ? x + r : x + w - r;
    int cy = top ? y + r : y + h - r;
    int dx = 2 * px + 1 - 2 * cx;
    int dy = 2 * py + 1 - 2 * cy;
    return dx * dx + dy * dy <= 4 * r * r;
}

}  // namespace more_menu_items
