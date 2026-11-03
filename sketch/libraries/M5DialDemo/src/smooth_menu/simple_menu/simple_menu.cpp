/**
 * @file simple_menu.cpp
 * @author Forairaaaaa
 * @brief 
 * @version 0.1
 * @date 2023-06-10
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "simple_menu.h"


namespace SMOOTH_MENU {


    void Simple_Menu::setMenuLoopMode(bool loopMode)
    {
        _selector->setMenuLoopMode(loopMode);
        _camera->setMenuLoopMode(loopMode);
    }


    Simple_Menu::~Simple_Menu()
    {
        _release_owned();
    }


    void Simple_Menu::_release_owned()
    {
        if (_owns_camera) delete _camera;
        if (_owns_selector) delete _selector;
        if (_owns_menu) delete _menu;
        _camera = nullptr;
        _selector = nullptr;
        _menu = nullptr;
        _owns_camera = _owns_selector = _owns_menu = false;
    }


    void Simple_Menu::init(Menu_t* menu, Selector_t* selector, Camrea_t* camera)
    {
        // why: a second init() would otherwise overwrite (and leak) the parts the first created
        _release_owned();
        _first_open = true;
        /* Init menu */
        _owns_menu = (menu == nullptr);
        if (menu == nullptr) {
            _menu = new Menu_t;
        }
        else {
            _menu = menu;
        }

        /* Init selector */
        _owns_selector = (selector == nullptr);
        if (selector == nullptr) {
            _selector = new Selector_t;
        }
        else {
            _selector = selector;
        }
        _selector->setMenu(_menu);

        /* Init camera */
        _owns_camera = (camera == nullptr);
        if (camera == nullptr) {
            _camera = new Camrea_t;
        }
        else {
            _camera = camera;
        }
        _camera->setMenu(_menu);
    }


    void Simple_Menu::update(uint32_t currentTime, bool renderAtOnce)
    {
        /* Move the selector to the first item */
        if (_first_open) {
            _first_open = false;
            _selector->goToItem(0);
            _selector->update(0);
        }

        _menu->update(currentTime);
        _selector->update(currentTime, false);
        _camera->update(currentTime, false);


        /* Render simple menu */
        if (renderAtOnce) {
            render();
        }
    }


    void Simple_Menu::render()
    {
        if (_render_callback == nullptr) {
            return;
        }

        _render_callback->renderCallback(
            _menu->getItemList(),
            _selector->getRenderAttribute(),
            _camera->getRenderAttribute()
        );


        

    }


    void Simple_Menu::goLast()
    {
        _selector->goLast();
        _camera->goLast();
    }


    void Simple_Menu::goNext()
    {
        _selector->goNext();
        _camera->goNext();
    }


}
