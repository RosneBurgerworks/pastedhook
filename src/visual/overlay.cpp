#include "visual/overlay.hpp"

#include <MiscTemporary.hpp>
#include <visual/SDLHooks.hpp>
#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <exception>
#include <menu/menu/Menu.hpp>
#if ENABLE_VISUALS
#include "drawmgr.hpp"
#endif

namespace overlay
{
static Api current_api{ Api::None };
static bool initialized{ false };
static bool failed{ false };

Api api()
{
    return current_api;
}

void frame(SDL_Window *window, Api frame_api)
{
    if (current_api == Api::None)
        current_api = frame_api;
    // Never mix renderers: resources were created for the first one
    if (frame_api != current_api)
        return;

    if (!sdl_hooks::window)
        sdl_hooks::window = window;

    if (!isHackActive() || disable_visuals || failed)
        return;

    try
    {
        int w = 0, h = 0;
        SDL_GetWindowSize(window, &w, &h);
        if (w > 0 && h > 0)
        {
            draw::width  = w;
            draw::height = h;
        }

        static int prev_width = 0, prev_height = 0;
        if (!initialized)
        {
            if (current_api == Api::GL)
            {
                const char *gl_ver = reinterpret_cast<const char *>(glGetString(GL_VERSION));
                logging::Info("SDL overlay: GL_VERSION=%s ctx=%p", gl_ver ? gl_ver : "?", SDL_GL_GetCurrentContext());
            }
            else
                logging::Info("DXVK overlay: window=%p %dx%d", window, w, h);
            draw::InitGL();
            initialized = true;
        }
        else if (zerokernel::Menu::instance && (draw::width != prev_width || draw::height != prev_height))
            zerokernel::Menu::instance->resize(draw::width, draw::height);
        prev_width  = draw::width;
        prev_height = draw::height;

        draw::BeginGL();
#if ENABLE_IMGUI_DRAWING
        render_cheat_visuals();
#else
        DrawCache();
#endif
        draw::EndGL();
    }
    catch (const std::exception &e)
    {
        logging::Info("overlay failed: %s — drawing disabled", e.what());
        failed = true;
    }
    catch (...)
    {
        logging::Info("overlay failed — drawing disabled");
        failed = true;
    }
}
} // namespace overlay
