/**
 * @file test_smooth_menu.cpp
 * @brief smooth_menu as the launcher configures it: 10 ring slots in loop mode, so stepping
 *        back from slot 0 wraps to slot 9 (an empty slot - the launcher has 8 icons) and
 *        stepping on from 9 wraps to 0. Uses the real smooth_menu sources (Arduino-free).
 */
#include "apps/launcher/launcher_layout.h"
#include "host_test.h"
#include "smooth_menu/simple_menu/simple_menu.h"

namespace {

/** Simple_Menu set up like Launcher::_menu_init() (ring of RING_SLOTS items, loop mode). */
struct LauncherMenu {
    SMOOTH_MENU::Simple_Menu menu;
    LauncherMenu() {
        menu.init(240, 240);
        menu.setMenuLoopMode(true);
        for (int i = 0; i < launcher_layout::RING_SLOTS; ++i) {
            launcher_layout::Point p = launcher_layout::ringPoint(i, launcher_layout::MENU_RING_R);
            menu.getMenu()->addItem("", p.x, p.y, 22, 22);
        }
        menu.update(0, false);
    }
    int target() { return static_cast<int>(menu.getSelector()->getTargetItem()); }
};

}  // namespace

HOST_TEST(menu_starts_at_slot_0) {
    LauncherMenu m;
    HT_EQ(0, m.target());
}

HOST_TEST(menu_go_next_advances) {
    LauncherMenu m;
    m.menu.goNext();
    HT_EQ(1, m.target());
}

HOST_TEST(menu_go_last_from_0_wraps_to_last_slot) {
    LauncherMenu m;
    m.menu.goLast();
    HT_EQ(launcher_layout::RING_SLOTS - 1, m.target());
    HT_EQ(9, m.target());
    m.menu.goNext();
    HT_EQ(0, m.target());
}

HOST_TEST(menu_go_to_item_selects_touched_icon) {
    LauncherMenu m;
    m.menu.getSelector()->goToItem(2);
    m.menu.getCamera()->goToItem(2);
    HT_EQ(2, m.target());
}

#include <cstring>
#include <new>

// why: Simple_Menu::init() news its Anim_Path members and the first update() calls getValue()
// before any resetTime(), so the constructor must set _time_offset (PR #31 review). Building it
// in junk-filled storage makes an uninitialised offset show up deterministically.
HOST_TEST(anim_path_starts_with_zero_time_offset) {
    alignas(LVGL::Anim_Path) unsigned char buf[sizeof(LVGL::Anim_Path)];
    std::memset(buf, 0x5A, sizeof(buf));
    auto* p = new (buf) LVGL::Anim_Path();
    p->getValue(1000);
    HT_EQ(1000, (int)p->getAnimValue()->act_time);
    p->~Anim_Path();
}
