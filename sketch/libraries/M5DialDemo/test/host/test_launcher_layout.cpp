/**
 * @file test_launcher_layout.cpp
 * @brief launcher_layout: the launcher's ring geometry (original launcher.cpp angle math,
 *        2 * 3.14 * i / 10 with int truncation) and the touch hit tests derived from it.
 *        Expected points were computed independently (Python, same formula).
 */
#include <cstring>

#include "apps/launcher/launcher_layout.h"
#include "host_test.h"

using namespace launcher_layout;

HOST_TEST(menu_ring_matches_original_points) {
    // original _menu_init(): a = b = 120, r = 60, n = 10
    const int expected[RING_SLOTS][2] = {{180, 120}, {168, 155}, {138, 177}, {101, 177},
                                         {71, 155},  {60, 120},  {71, 84},   {101, 62},
                                         {138, 62},  {168, 84}};
    for (int i = 0; i < RING_SLOTS; ++i) {
        Point p = ringPoint(i, MENU_RING_R);
        HT_EQ(expected[i][0], p.x);
        HT_EQ(expected[i][1], p.y);
    }
}

HOST_TEST(icon_ring_matches_original_points) {
    // original _icon_list_init(): a = b = 120, r = 190 / 2, n = 10
    const int expected[RING_SLOTS][2] = {{215, 120}, {196, 175}, {149, 210}, {90, 210},
                                         {43, 175},  {25, 120},  {43, 64},   {90, 29},
                                         {149, 29},  {196, 63}};
    for (int i = 0; i < RING_SLOTS; ++i) {
        Point p = ringPoint(i, ICON_RING_R);
        HT_EQ(expected[i][0], p.x);
        HT_EQ(expected[i][1], p.y);
    }
}

HOST_TEST(center_hit_includes_boundary) {
    // original: `if ((x * x + y * y) > (50 * 50)) return;` -> boundary counts as inside
    HT_EQ(true, inCenter(120, 120));
    HT_EQ(true, inCenter(170, 120));
    HT_EQ(false, inCenter(171, 120));
    HT_EQ(true, inCenter(120, 70));
    HT_EQ(true, inCenter(155, 155));   // 35^2 + 35^2 = 2450
    HT_EQ(false, inCenter(156, 156));  // 36^2 + 36^2 = 2592
}

HOST_TEST(icon_at_each_icon_centre) {
    for (int i = 0; i < ICON_COUNT; ++i) {
        Point p = ringPoint(i, ICON_RING_R);
        HT_EQ(i, iconAt(p.x, p.y));
    }
}

HOST_TEST(icon_at_empty_slots_and_centre_is_none) {
    // slots 8 and 9 exist in the menu but carry no icon ("-. -")
    Point s8 = ringPoint(8, ICON_RING_R);
    Point s9 = ringPoint(9, ICON_RING_R);
    HT_EQ(-1, iconAt(s8.x, s8.y));
    HT_EQ(-1, iconAt(s9.x, s9.y));
    HT_EQ(-1, iconAt(120, 120));
    HT_EQ(-1, iconAt(0, 0));
    HT_EQ(-1, iconAt(-5, 300));
}

HOST_TEST(icon_at_hit_radius_edge) {
    // icon 0 at (215, 120), hit radius ICON_HIT_R = 22 (the drawn icon circle)
    HT_EQ(0, iconAt(215 - 22, 120));
    HT_EQ(-1, iconAt(215 - 23, 120));
    HT_EQ(0, iconAt(215, 120 + 22));
    HT_EQ(-1, iconAt(215, 120 + 23));
}

HOST_TEST(app_names_cover_icons_and_empty_slots) {
    HT_EQ(0, std::strcmp("LcdTest", appName(0)));
    HT_EQ(0, std::strcmp("RtcTest", appName(1)));
    HT_EQ(0, std::strcmp("RfidTest", appName(2)));
    HT_EQ(0, std::strcmp("Brightness", appName(3)));
    HT_EQ(0, std::strcmp("WifiScan", appName(4)));
    HT_EQ(0, std::strcmp("BleServer", appName(5)));
    HT_EQ(0, std::strcmp("TempDemo", appName(6)));
    HT_EQ(0, std::strcmp("MoreMenu", appName(7)));
    HT_EQ(0, std::strcmp("-", appName(8)));
    HT_EQ(0, std::strcmp("-", appName(9)));
    HT_EQ(0, std::strcmp("-", appName(-1)));
}
