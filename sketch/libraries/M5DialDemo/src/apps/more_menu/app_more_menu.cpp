/**
 * @file app_more_menu.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2023-08-07
 *
 * @copyright Copyright (c) 2023
 *
 * M5DialDemo port: every HAL call replaced by the M5Dial API (docs/PORTING_NOTES.md,
 * "More menu / power-off"), entries from more_menu_items.h without the LVGL demos,
 * M5Dial.update() in the button wait, M5_LOGI log lines.
 */
#include "app_more_menu.h"

#include <M5Dial.h>

#include "../../dial_encoder.h"
#include "../../m5dd_begin.h"
#include "../../ui_tones.h"
#include "more_menu_items.h"
#include "more_menu_render_callback.hpp"


using namespace MOONCAKE::USER_APP;

namespace {
constexpr int32_t SELECTOR_ANIM_MS = 400;  ///< original overshoot time of the selector
}


void MoreMenu::_log_select()
{
    int target = static_cast<int>(_data.menu->getSelector()->getTargetItem());
    M5_LOGI("more: select %d %s", target, more_menu_items::tag(target));
}


void MoreMenu::_create_menu()
{
    /* Create a menu to handle selector */
    _data.menu = new SMOOTH_MENU::Simple_Menu;
    _data.menu_render_cb = new MoreMenuRender_CB_t;
    _data.menu_render_cb->setCanvas(_data.ctx->canvas);

    _data.menu->init(240, 240);
    _data.menu->setRenderCallback(_data.menu_render_cb);

    /* Set selector anim */
    auto cfg_selector = _data.menu->getSelector()->config();
    cfg_selector.animPath_x = LVGL::overshoot;
    cfg_selector.animPath_y = LVGL::overshoot;
    cfg_selector.animTime_x = SELECTOR_ANIM_MS;
    cfg_selector.animTime_y = SELECTOR_ANIM_MS;
    _data.menu->getSelector()->config(cfg_selector);

    /* Set menu loop mode */
    _data.menu->setMenuLoopMode(false);


    /* Push item into menu */
    for (int i = 0; i < more_menu_items::ITEM_COUNT; i++)
    {
        more_menu_items::Rect r = more_menu_items::itemRect(i);
        _data.menu->getMenu()->addItem(more_menu_items::tag(i), r.x, r.y, r.w, r.h);
    }
}


void MoreMenu::_menu_loop()
{
    // why no extra pushSprite (upstream _canvas_update()): the render callback pushes the frame
    _data.menu->update(millis());

    /* If scrolled */
    // why a loop: a fast turn can pass several detents between two frames (Review Focus 2);
    // clockwise = next entry (Review Focus 3), as upstream's getDirection() < 1 -> goNext()
    int32_t steps = m5dd::readDetentsWithClick(_detent);
    for (; steps > 0; --steps)
    {
        _data.menu->goNext();
        _log_select();
    }
    for (; steps < 0; ++steps)
    {
        _data.menu->goLast();
        _log_select();
    }

    /* If button pressed */
    // why wasPressed (edge): the click that opened the More menu in the launcher must not
    // choose "Quit" at once (same rule as the launcher)
    if (M5Dial.BtnA.wasPressed())
    {
        M5Dial.Speaker.tone(ui_tones::TONE_PRESS_HZ, ui_tones::TONE_MS);
        _data.menu->getSelector()->pressed();

        /* Hold until button release */
        // why M5Dial.update(): BtnA only changes state inside update() (Review Focus 1)
        while (M5Dial.BtnA.isPressed())
        {
            M5Dial.update();
            _data.menu->update(millis());
        }

        _data.menu->getSelector()->released();

        /* Hold until anim finish */
        // why no M5Dial.update() here: this waits on the 400 ms animation, not on input
        while (!_data.menu->getSelector()->isAnimFinished())
        {
            _data.menu->update(millis());
        }

        /* App open callback */
        _item_selected_callback(_data.menu->getSelector()->getTargetItem());
    }
}


MoreMenu::~MoreMenu()
{
    // why here as well: a host that deletes the app without onDestroy() must not leak
    delete _data.menu;
    delete _data.menu_render_cb;
}


void MoreMenu::onSetup()
{
    setAppName("MoreMenu");
    setAllowBgRunning(false);

    /* Copy default value */
    MORE_MENU::Data_t default_data;
    _data = default_data;

    _data.ctx = static_cast<AppContext*>(getUserData());
}


/* Life cycle */
void MoreMenu::onCreate()
{
    M5_LOGI("app: open %s", getAppName().c_str());
    _create_menu();
    _detent.reset(dialEncoder.read());
}


void MoreMenu::onRunning()
{
    _menu_loop();
}


void MoreMenu::onDestroy()
{
    delete _data.menu;
    delete _data.menu_render_cb;
    _data.menu = nullptr;
    _data.menu_render_cb = nullptr;
    M5_LOGI("app: close %s", getAppName().c_str());
}
