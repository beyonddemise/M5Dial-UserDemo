/**
 * @file app.h
 * @author Forairaaaaa
 * @brief 
 * @version 0.1
 * @date 2023-05-07
 * 
 * @copyright Copyright (c) 2023
 * 
 * Ported to Arduino for M5DialDemo from main/apps/app.h (M5Stack M5Dial-UserDemo, MIT):
 * API unchanged; only the gui_base include path and doc comments differ.
 */
#pragma once
// #include <Arduino.h>
#include <iostream>
#include <string>
#include "gui_base.h"


namespace MOONCAKE {


    /**
     * @brief Base class of every app: inherit and override the "Life cycle" callbacks.
     *
     * Standalone sketches drive it via m5dd::runStandalone(); the launcher (UserDemo) via
     * its app manager. The user data pointer is the sketch's AppContext.
     */
    class APP_BASE {
        private:
            std::string _name;
            void* _user_data;
            void* _icon_addr;
            bool _allow_bg_running;
            bool _go_close;
            bool _go_destroy;


        protected:
            /* App internal API */

            /**
             * @brief Set the App Name
             * 
             * @param name 
             */
            inline void setAppName(const std::string& name) { _name = name; }

            /**
             * @brief Set the App Icon
             * 
             * @param icon 
             */
            inline void setAppIcon(void* icon) { _icon_addr = icon; }

            /**
             * @brief Set if is App running background after closed
             * 
             * @param allow 
             */
            inline void setAllowBgRunning(bool allow) { _allow_bg_running = allow; }

            /**
             * @brief Close App 
             * 
             */
            inline void closeApp() { _go_close = true; }

            /**
             * @brief Destroy App, not going background
             * 
             */
            inline void destroyApp() { _go_destroy = true; }

            /**
             * @brief Get user data
             * 
             * @return void* 
             */
            inline void* getUserData() { return _user_data; };


        public:
            /** @brief Construct an unnamed App without user data, icon or pending requests */
            APP_BASE() :
                _name(""),
                _user_data(nullptr),
                _icon_addr(nullptr),
                _allow_bg_running(false),
                _go_close(false),
                _go_destroy(false) {}
            virtual ~APP_BASE() {}


            /* API for App manager */
            /** @brief Set the user data returned by getUserData() (an AppContext*) */
            inline void setUserData(void* userData) { _user_data = userData; }

            /**
             * @brief Get the Gui object
             * 
             * @return GUI_Base* 
             */
            virtual GUI_Base* getGui() { return nullptr; }

            
            /* Basic API */
            /** @brief App name set by setAppName() */
            inline std::string getAppName() { return _name; }
            /** @brief App icon set by setAppIcon() */
            inline void* getAppIcon() { return _icon_addr; }
            /** @brief True if the App keeps running in the background after close */
            inline bool isAllowBgRunning() { return _allow_bg_running; }
            /** @brief True after the App called closeApp() */
            inline bool isGoingClose() { return _go_close; }
            /** @brief True after the App called destroyApp() */
            inline bool isGoingDestroy() { return _go_destroy; }
            /** @brief Clear the close request */
            inline void resetGoingCloseFlag() { _go_close = false; }
            /** @brief Clear the destroy request */
            inline void resetGoingDestroyFlag() { _go_destroy = false; }


            /**
             * @brief Lifecycle callbacks for derived to override
             * 
             */
            /* Setup App configs, called when App "install()" */
            virtual void onSetup() {}

            /* Life cycle */
            /** @brief Called once when the App is opened */
            virtual void onCreate() {}
            /** @brief Called when the App comes back to the foreground */
            virtual void onResume() {}
            /** @brief Called repeatedly while the App is in the foreground */
            virtual void onRunning() {}
            /** @brief Called repeatedly while the App runs in the background */
            virtual void onRunningBG() {}
            /** @brief Called when the App goes to the background */
            virtual void onPause() {}
            /** @brief Called once when the App is closed for good */
            virtual void onDestroy() {}

    };


}