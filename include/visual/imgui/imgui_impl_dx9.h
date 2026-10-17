#pragma once

#include "imgui.h"

// Direct3D 9 renderer for the overlay, used when TF2 runs on DXVK (-vulkan).
// The device pointer is the IDirect3DDevice9 passed to the hooked Present.

void ImGui_ImplDX9_SetDevice(void *device);
void ImGui_ImplDX9_NewFrame();
void ImGui_ImplDX9_GetBackBufferSize(int *width, int *height);
void ImGui_ImplDX9_Render(ImDrawData *draw_data);
ImTextureID ImGui_ImplDX9_CreateTexture(const unsigned char *rgba, int width, int height, bool linear);
void ImGui_ImplDX9_DestroyTexture(ImTextureID texture);
// Releases every D3DPOOL_DEFAULT resource; call before IDirect3DDevice9::Reset.
void ImGui_ImplDX9_InvalidateDeviceObjects();
