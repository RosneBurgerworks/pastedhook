#pragma once

// Overlay hooks for TF2's -vulkan renderer (shaderapivk -> DXVK d3d9 -> Vulkan).
// Does nothing when libdxvk_d3d9.so is not loaded, i.e. under OpenGL.
namespace d3d9_hooks
{
void apply();
void clean();
} // namespace d3d9_hooks
