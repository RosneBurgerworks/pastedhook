/*
  Created by Jenny White on 29.04.18.
  Copyright (c) 2018 nullworks. All rights reserved.
*/

#include <visual/SDLHooks.hpp>
#include <visual/overlay.hpp>
#include "HookedMethods.hpp"
#include <SDL2/SDL.h>
#if ENABLE_CLIP
#include "clip.h"
#endif

namespace hooked_methods
{
#if ENABLE_CLIP
DEFINE_HOOKED_METHOD(SDL_SetClipboardText, int, const char *text)
{
    if (original::SDL_SetClipboardText)
        original::SDL_SetClipboardText(text);
    clip::set_text(text);
    return 0;
}
#endif

DEFINE_HOOKED_METHOD(SDL_GL_SwapWindow, void, SDL_Window *window)
{
    static thread_local bool reentrant = false;
    auto *orig                         = original::SDL_GL_SwapWindow;
    if (!orig)
        return;

    if (reentrant)
    {
        orig(window);
        return;
    }
    reentrant = true;

    overlay::frame(window, overlay::Api::GL);

    orig(window);
    reentrant = false;
}
} // namespace hooked_methods
