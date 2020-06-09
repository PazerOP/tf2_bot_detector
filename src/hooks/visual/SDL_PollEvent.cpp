/*
  Created by Jenny White on 29.04.18.
  Copyright (c) 2018 nullworks. All rights reserved.
*/

#include <menu/GuiInterface.hpp>
#include "HookedMethods.hpp"
#include "MiscTemporary.hpp"
namespace hooked_methods
{

DEFINE_HOOKED_METHOD(SDL_PollEvent, int, SDL_Event *event)
{
    auto ret = original::SDL_PollEvent(event);
#if ENABLE_GUI
    if (!isHackActive())
        return ret;
    static Timer waitfirst{};
    if (!ignoreKeys && gui::handleSdlEvent(event))
        return 0;
    g_IEngine->GetScreenSize(draw::width, draw::height);
#endif
    return ret;
}
} // namespace hooked_methods
