/**
 * @file launcher.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2023-07-25
 *
 * @copyright Copyright (c) 2023
 *
 * M5DialDemo port: every HAL call replaced by the M5Dial API (docs/PORTING_NOTES.md,
 * "Launcher"), ring math moved to launcher_layout.h (host-tested), touch on an icon selects
 * it, M5Dial.update() in every input wait, M5_LOGI log lines.
 */
#include "launcher.h"

#include <M5Dial.h>
#include <esp_heap_caps.h>

#include "../../dial_encoder.h"
#include "../../ui_tones.h"
#include "../lcd_test/app_lcd_test.h"
#include "../rfid_test/app_rfid_test.h"
#include "../rtc_test/app_rtc_test.h"
#include "../set_brightness/app_set_brightness.h"
#include "../temp_demo/app_temp_demo.h"
#include "launcher_layout.h"
#include "launcher_render_callback.hpp"


using namespace MOONCAKE::USER_APP;

static_assert(launcher_layout::ICON_COUNT == ICON_NUM, "layout and icon tables disagree");
static_assert(launcher_layout::ICON_HIT_R == ICON_RADIUS, "touch radius = drawn icon radius");

namespace {

// why: the original HAL played the click tones from its encoder-moved and button-pressed
// callbacks (hal.cpp); without a HAL the launcher plays them itself (values: ui_tones.h).
using namespace ui_tones;

constexpr int ICON_SIZE = 42;        ///< launcher icon bitmaps are 42x42 RGB565
constexpr int APP_OPEN_FRAMES = 10;  ///< circle-wipe frames of the open/close animation
constexpr int APP_ANIM_STEP_R = 24;  ///< circle radius growth per frame

}  // namespace


void Launcher::_canvas_update()
{
    _data.ctx->canvas->pushSprite(0, 0);
}


void Launcher::_log_select()
{
    int target = static_cast<int>(_data.menu->getSelector()->getTargetItem());
    M5_LOGI("launcher: select %d %s", target, launcher_layout::appName(target));
}


void Launcher::_menu_init()
{
    /* Create a menu to handle selector */
    _data.menu = new SMOOTH_MENU::Simple_Menu;
    _data.menu_render_cb = new LauncherRender_CB_t;
    _data.menu_render_cb->setCanvas(_data.ctx->canvas);

    _data.menu->init(240, 240);
    _data.menu->setRenderCallback(_data.menu_render_cb);

    /* Set selector anim */
    auto cfg_selector = _data.menu->getSelector()->config();
    cfg_selector.animPath_x = LVGL::overshoot;
    cfg_selector.animPath_y = LVGL::overshoot;
    cfg_selector.animTime_x = 300;
    cfg_selector.animTime_y = 300;
    _data.menu->getSelector()->config(cfg_selector);

    /* Set menu looply */
    _data.menu->setMenuLoopMode(true);
    _data.menu->getMenu()->x = 0;
    _data.menu->getMenu()->y = 0;

    /* Push selector points into menu */
    for (int i = 0; i < launcher_layout::RING_SLOTS; i++)
    {
        launcher_layout::Point p = launcher_layout::ringPoint(i, launcher_layout::MENU_RING_R);
        _data.menu->getMenu()->addItem("", p.x, p.y, ICON_RADIUS, ICON_RADIUS);
    }
}


void Launcher::_icon_list_init()
{
    for (size_t i = 0; i < icon_list.size(); i++)
    {
        /* Set colors */
        icon_list[i].color = launcher_layout::appColor(static_cast<int>(i));

        /* Set tags */
        icon_list[i].tag_up = icon_tag_list[i * 2];
        icon_list[i].tag_down = icon_tag_list[i * 2 + 1];

        /* Push Icon pic into sprite */
        // why no setSwapBytes: the icon headers hold byte-swapped RGB565 (lcd-image-converter
        // output, as upstream); M5GFX, like the original LovyanGFX, reads `const uint16_t*`
        // as swap565_t while swapBytes is false (the default) - so the colours match.
        icon_sprite_list[i].createSprite(ICON_SIZE, ICON_SIZE);
        icon_sprite_list[i].pushImage(0, 0, ICON_SIZE, ICON_SIZE, icon_pic_list[i]);
    }

    /* Icon position */
    for (size_t i = 0; i < icon_list.size(); i++)
    {
        launcher_layout::Point p =
            launcher_layout::ringPoint(static_cast<int>(i), launcher_layout::ICON_RING_R);
        icon_list[i].x = p.x;
        icon_list[i].y = p.y;
    }
}


void Launcher::_launcher_init()
{
    _menu_init();
    _icon_list_init();
}


void Launcher::_launcher_loop()
{
    _data.menu->update(millis());
    _canvas_update();

    /* If scrolled */
    // why a loop: the original moved one item per wasMoved(); a fast turn can pass several
    // detents between two frames, and each one must move the selector (Review Focus 2).
    int32_t steps = _detent.update(dialEncoder.read());
    for (; steps > 0; --steps)
    {
        _data.menu->goNext();
        M5Dial.Speaker.tone(TONE_NEXT_HZ, TONE_MS);
        _log_select();
    }
    for (; steps < 0; ++steps)
    {
        _data.menu->goLast();
        M5Dial.Speaker.tone(TONE_LAST_HZ, TONE_MS);
        _log_select();
    }

    /* If button pressed */
    // why wasPressed (edge) instead of the original's level read: a button still held when
    // the launcher (re)starts - after the factory-test hold or an app that quits on press -
    // must not open an app again.
    if (M5Dial.BtnA.wasPressed())
    {
        M5Dial.Speaker.tone(TONE_PRESS_HZ, TONE_MS);
        _data.menu->getSelector()->pressed();

        /* Hold until button release */
        // why M5Dial.update(): BtnA only changes state inside update() (Review Focus 1)
        while (M5Dial.BtnA.isPressed())
        {
            M5Dial.update();
            _data.menu->update(millis());
            _canvas_update();
        }

        _data.menu->getSelector()->released();

        /* App open callback */
        _app_open_callback(_data.menu->getSelector()->getTargetItem());
    }

    /* If touched */
    auto touch = M5Dial.Touch.getDetail();
    if (touch.isPressed())
    {
        /* Check if in the center circle (r = 50) */
        if (!launcher_layout::inCenter(touch.x, touch.y))
        {
            // why: not in the original (it ignored touches outside the centre); a new tap on
            // an icon selects it, so the ring is usable by touch alone (plan Task 6)
            if (touch.wasPressed())
            {
                int icon = launcher_layout::iconAt(touch.x, touch.y);
                if (icon >= 0)
                {
                    _data.menu->getSelector()->goToItem(icon);
                    _data.menu->getCamera()->goToItem(icon);
                    _log_select();
                }
            }
            return;
        }

        /* Button pressed feedback (the original called the HAL's button callback) */
        M5Dial.Speaker.tone(TONE_PRESS_HZ, TONE_MS);

        if (_data.menu->getSelector()->isAnimFinished())
        {
            /* App open callback */
            _app_open_callback(_data.menu->getSelector()->getTargetItem());

            /* Wait until released */
            while (M5Dial.Touch.getDetail().isPressed())
            {
                M5Dial.update();
                _data.menu->update(millis());
                _canvas_update();
            }
        }
    }
}


void Launcher::_app_open_callback(uint8_t selectedNum)
{
    /* If not in icon list */
    if (selectedNum >= icon_list.size())
    {
        return;
    }


    /* Special color for app more menu */
    uint32_t theme_color = 0;
    if (selectedNum != 7)
        theme_color = icon_list[selectedNum].color;
    else
        theme_color = 0;


    /* Play app open anim */
    for (int i = 0; i < APP_OPEN_FRAMES; i++)
    {
        _data.ctx->canvas->fillSmoothCircle(icon_list[selectedNum].x, icon_list[selectedNum].y, i * APP_ANIM_STEP_R, theme_color);
        _canvas_update();
    }


    /* Memery leak check */
    size_t mem_before_open = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    M5_LOGI("launcher: free mem %u", (unsigned)mem_before_open);


    MOONCAKE::APP_BASE* app_ptr = nullptr;

    /* Create app */
    // why some cases are empty: each app is added here as its port lands (original class in
    // the comment); until then the entry logs `app: <name> not ported` below.
    switch (selectedNum)
    {
        case launcher_layout::SLOT_LCD_TEST:
            app_ptr = new MOONCAKE::USER_APP::LCD_Test;
            break;
        case launcher_layout::SLOT_RTC_TEST:
            app_ptr = new MOONCAKE::USER_APP::RTC_Test;
            break;
        case launcher_layout::SLOT_RFID_TEST:
            app_ptr = new MOONCAKE::USER_APP::RFID_Test;
            break;
        case launcher_layout::SLOT_BRIGHTNESS:
            app_ptr = new MOONCAKE::USER_APP::Set_Brightness;
            break;
        case 4:  // MOONCAKE::USER_APP::WiFi_Scan
            break;
        case 5:  // MOONCAKE::USER_APP::BLE_Server
            break;
        case launcher_layout::SLOT_TEMP_DEMO:  // upstream VideoShit
            app_ptr = new MOONCAKE::USER_APP::TempDemo;
            break;
        case 7:  // MOONCAKE::USER_APP::MoreMenu
            break;
        default:
            break;
    };

    /* If app created */
    if (app_ptr != nullptr)
    {
        /* Init if gui module exsit */
        if (app_ptr->getGui() != nullptr)
        {
            app_ptr->getGui()->setThemeColor(icon_list[selectedNum].color);
            app_ptr->getGui()->init(_data.ctx->canvas, &icon_sprite_list[selectedNum]);
        }

        /* Run app */
        _simple_app_manager(app_ptr);

        /* Free app */
        delete app_ptr;
    }
    else
    {
        M5_LOGI("app: %s not ported", launcher_layout::appName(selectedNum));
    }

    // why: the knob may have turned while the app ran (the app consumed those detents);
    // re-base so the launcher does not jump on return (the original's wasMoved() shared its
    // last count with the apps for the same effect)
    _detent.reset(dialEncoder.read());


    /* Memery leak check */
    size_t mem_after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    M5_LOGI("launcher: free mem %u", (unsigned)mem_after);
    if (mem_after < mem_before_open)
    {
        M5_LOGE("launcher: memory leak %u", (unsigned)(mem_before_open - mem_after));
    }



    /* Play app close anim */
    for (int i = APP_OPEN_FRAMES; i > 1; i--)
    {
        _data.menu->update(millis());
        _data.ctx->canvas->fillSmoothCircle(icon_list[selectedNum].x, icon_list[selectedNum].y, i * APP_ANIM_STEP_R, theme_color);
        _canvas_update();
    }

    M5_LOGI("launcher: ready");
}


void Launcher::_simple_app_manager(MOONCAKE::APP_BASE* app)
{
    app->setUserData((void*)_data.ctx);
    app->onSetup();
    app->onCreate();
    while (1)
    {
        // why: apps read input like standalone sketches do (m5dd::runStandalone), so the
        // host refreshes the M5Dial state before every frame
        M5Dial.update();
        app->onRunning();
        // why also isGoingClose(): the original only honoured destroyApp(); runStandalone()
        // accepts both, so an app behaves the same in the launcher and standalone
        if (app->isGoingDestroy() || app->isGoingClose())
        {
            app->resetGoingDestroyFlag();
            app->resetGoingCloseFlag();
            app->onDestroy();
            break;
        }
    }
}


Launcher::~Launcher()
{
    delete _data.menu;
    delete _data.menu_render_cb;
}


void Launcher::onSetup()
{
    setAppName("Launcher");
    setAllowBgRunning(false);

    /* Init with default value */
    LAUNCHER::Data_t default_data;
    _data = default_data;

    _data.ctx = static_cast<AppContext*>(getUserData());
}


/* Life cycle */
void Launcher::onCreate()
{
    _launcher_init();
    _data.menu->getSelector()->reset(millis());
    _detent.reset(dialEncoder.read());
    M5_LOGI("launcher: ready");
}


void Launcher::onRunning()
{
    _launcher_loop();
}
