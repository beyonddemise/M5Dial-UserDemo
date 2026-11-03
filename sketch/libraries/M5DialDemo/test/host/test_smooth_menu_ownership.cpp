/**
 * @file test_smooth_menu_ownership.cpp
 * @brief smooth_menu frees what it allocated: Simple_Menu the menu, selector and camera its
 *        init() created (not ones passed in), Menu_t its items. MoreMenu creates and destroys
 *        a Simple_Menu on every open, so a leak here costs heap on every return to the launcher.
 *
 * why a global operator new/delete replacement: it is the only way to see the library's own
 * allocations from outside. It applies to the whole host-test binary, but only counts; the
 * count is read as a difference around the code under test.
 */
#include <cstdlib>
#include <new>

#include "host_test.h"
#include "smooth_menu/simple_menu/simple_menu.h"

namespace {
long live_allocations = 0;  ///< operator new calls minus operator delete calls (non-null)

void* counted_alloc(std::size_t size) {
    void* p = std::malloc(size ? size : 1);
    if (p == nullptr) throw std::bad_alloc();
    ++live_allocations;
    return p;
}

void counted_free(void* p) noexcept {
    if (p == nullptr) return;
    --live_allocations;
    std::free(p);
}

constexpr int MORE_MENU_ITEMS = 13;  ///< as MoreMenu::_create_menu()

/** Fills `menu` the way MoreMenu::_create_menu() does and renders one frame. */
void build_like_more_menu(SMOOTH_MENU::Simple_Menu& menu) {
    menu.init(240, 240);
    for (int i = 0; i < MORE_MENU_ITEMS; ++i) menu.getMenu()->addItem("item", 20, 30 * i, 200, 24);
    menu.update(0, false);
}
}  // namespace

void* operator new(std::size_t size) { return counted_alloc(size); }
void* operator new[](std::size_t size) { return counted_alloc(size); }
void operator delete(void* p) noexcept { counted_free(p); }
void operator delete[](void* p) noexcept { counted_free(p); }
void operator delete(void* p, std::size_t) noexcept { counted_free(p); }
void operator delete[](void* p, std::size_t) noexcept { counted_free(p); }

HOST_TEST(simple_menu_frees_menu_selector_camera_and_items_it_allocated) {
    long before = live_allocations;
    auto* menu = new SMOOTH_MENU::Simple_Menu;
    build_like_more_menu(*menu);
    HT_EQ(true, live_allocations > before);
    delete menu;
    HT_EQ(0L, live_allocations - before);
}

HOST_TEST(simple_menu_open_close_repeated_does_not_grow_the_heap) {
    long before = live_allocations;
    for (int i = 0; i < 5; ++i) {
        SMOOTH_MENU::Simple_Menu menu;
        build_like_more_menu(menu);
    }
    HT_EQ(0L, live_allocations - before);
}

HOST_TEST(simple_menu_leaves_parts_passed_to_init_to_the_caller) {
    auto* own_menu = new SMOOTH_MENU::Menu_t;
    auto* own_selector = new SMOOTH_MENU::Selector_t;
    auto* own_camera = new SMOOTH_MENU::Camrea_t;
    long with_parts = live_allocations;
    {
        SMOOTH_MENU::Simple_Menu menu;
        menu.init(own_menu, own_selector, own_camera);
    }
    HT_EQ(0L, live_allocations - with_parts);  // nothing of the caller's was freed
    delete own_camera;
    delete own_selector;
    delete own_menu;
}

HOST_TEST(menu_clear_all_item_frees_the_items) {
    SMOOTH_MENU::Menu_t menu;
    long before = live_allocations;
    for (int i = 0; i < MORE_MENU_ITEMS; ++i) menu.addItem("item", 0, i, 10, 10);
    menu.clearAllItem();
    HT_EQ(0, (int)menu.getItemList().size());
    HT_EQ(0L, live_allocations - before);
}

HOST_TEST(simple_menu_init_twice_frees_the_first_parts) {
    long before = live_allocations;
    {
        SMOOTH_MENU::Simple_Menu menu;
        build_like_more_menu(menu);
        build_like_more_menu(menu);  // re-init: the first menu, selector and camera must go
        HT_EQ(MORE_MENU_ITEMS, (int)menu.getMenu()->getItemNum());
    }
    HT_EQ(0L, live_allocations - before);
}

HOST_TEST(menu_takes_ownership_of_an_item_passed_by_pointer) {
    long before = live_allocations;
    {
        SMOOTH_MENU::Menu_t menu;
        HT_EQ(0, menu.addItem(new SMOOTH_MENU::Item_t));
        HT_EQ(-1, menu.addItem(nullptr));
    }
    HT_EQ(0L, live_allocations - before);  // freed by ~Menu_t, as documented on addItem()
}
