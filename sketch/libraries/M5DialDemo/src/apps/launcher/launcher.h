/**
 * @file launcher.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2023-07-25
 *
 * @copyright Copyright (c) 2023
 *
 * M5DialDemo port: HAL -> AppContext + M5Dial API (no compatibility layer), app headers are
 * added to the factory as each app is ported; the render callback is forward-declared so the
 * launcher's static icon tables stay in launcher.cpp.
 */
#pragma once
#include <stdint.h>

#include "../../app_context.h"
#include "../../encoder_detent.h"
#include "../../mooncake/app.h"
#include "../../smooth_menu/simple_menu/simple_menu.h"

struct LauncherRender_CB_t;


namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace LAUNCHER
        {
            /** @brief Launcher runtime state */
            struct Data_t
            {
                AppContext* ctx = nullptr;                    ///< host context (canvas)
                SMOOTH_MENU::Simple_Menu* menu = nullptr;
                LauncherRender_CB_t* menu_render_cb = nullptr;
            };
        }

        /**
         * @brief The UserDemo home screen: a ring of app icons, navigated with the encoder
         * (clockwise = next), opened with the button or a tap on the centre; a tap on an icon
         * selects it. Runs the chosen app in place until it destroys itself.
         *
         * Log lines: `launcher: ready`, `launcher: select <index> <name>`,
         * `app: <name> not ported` (factory entries of apps not ported yet).
         * User data: AppContext* (set with setUserData() before onSetup()).
         */
        class Launcher : public APP_BASE
        {
            private:
                LAUNCHER::Data_t _data;
                EncoderDetent _detent;  ///< encoder raw count -> detent steps (cw positive)
                void _menu_init();
                void _icon_list_init();
                void _launcher_init();
                void _launcher_loop();
                void _app_open_callback(uint8_t selectedNum);
                void _simple_app_manager(MOONCAKE::APP_BASE* app);
                void _canvas_update();
                void _log_select();

            public:
                Launcher() = default;
                ~Launcher();


                /**
                 * @brief Lifecycle callbacks for derived to override
                 *
                 */
                /** @brief Setup App configs: name, reads the AppContext from user data */
                void onSetup() override;

                /** @brief Build menu and icons, then log `launcher: ready` */
                void onCreate() override;
                // void onResume();
                /** @brief One launcher frame: render, then handle encoder, button and touch.
                 *  The host calls M5Dial.update() before each call. */
                void onRunning() override;
                // void onRunningBG();
                // void onPause();
                // void onDestroy();
        };

    }
}
