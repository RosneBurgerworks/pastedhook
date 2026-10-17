#pragma once

struct SDL_Window;

namespace overlay
{
// Graphics API the overlay renders with; fixed by whichever present hook
// fires first (SDL_GL_SwapWindow for OpenGL, IDirect3DDevice9::Present for DXVK)
enum class Api
{
    None,
    GL,
    DX9
};

Api api();

// One overlay frame: init on first call, resize, draw. Call before presenting.
void frame(SDL_Window *window, Api frame_api);
} // namespace overlay
