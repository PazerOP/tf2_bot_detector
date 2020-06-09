/*
  Created by Jenny White on 29.04.18.
  Copyright (c) 2018 nullworks. All rights reserved.
*/

#include "SDLHooks.hpp"
#include "HookedMethods.hpp"

namespace sdl_hooks
{

SDL_Window *window{ nullptr };

namespace pointers
{
#if ENABLE_CLIP
static hooked_methods::types::SDL_SetClipboardText *SDL_SetClipboardText{ nullptr };
#endif
static hooked_methods::types::SDL_GL_SwapWindow *SDL_GL_SwapWindow{ nullptr };
static hooked_methods::types::SDL_PollEvent *SDL_PollEvent{ nullptr };
} // namespace pointers

void applySdlHooks()
{
    pointers::SDL_GL_SwapWindow                 = reinterpret_cast<hooked_methods::types::SDL_GL_SwapWindow *>(sharedobj::libsdl().Pointer(0xFD648));
    hooked_methods::original::SDL_GL_SwapWindow = *pointers::SDL_GL_SwapWindow;
    *pointers::SDL_GL_SwapWindow                = hooked_methods::methods::SDL_GL_SwapWindow;

    pointers::SDL_PollEvent                 = reinterpret_cast<hooked_methods::types::SDL_PollEvent *>(sharedobj::libsdl().Pointer(0xFCF64));
    hooked_methods::original::SDL_PollEvent = *pointers::SDL_PollEvent;
    *pointers::SDL_PollEvent                = hooked_methods::methods::SDL_PollEvent;
#if ENABLE_CLIP
    pointers::SDL_SetClipboardText                 = reinterpret_cast<hooked_methods::types::SDL_SetClipboardText *>(sharedobj::libsdl().Pointer(0xFCF04));
    hooked_methods::original::SDL_SetClipboardText = *pointers::SDL_SetClipboardText;
    *pointers::SDL_SetClipboardText                = hooked_methods::methods::SDL_SetClipboardText;
#endif
}

void cleanSdlHooks()
{
    *pointers::SDL_GL_SwapWindow = hooked_methods::original::SDL_GL_SwapWindow;
    *pointers::SDL_PollEvent     = hooked_methods::original::SDL_PollEvent;
#if ENABLE_CLIP
    *pointers::SDL_SetClipboardText = hooked_methods::original::SDL_SetClipboardText;
#endif
}
} // namespace sdl_hooks
