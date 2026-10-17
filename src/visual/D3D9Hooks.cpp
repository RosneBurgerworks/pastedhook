/*
 * D3D9Hooks.cpp
 *
 * TF2 -vulkan loads shaderapivk.so, which draws through DXVK's libdxvk_d3d9.so.
 * SDL_GL_SwapWindow is never called there, so the overlay hooks DXVK's
 * IDirect3DDevice9 vtable instead: Present/PresentEx draw the overlay,
 * Reset/ResetEx release our D3DPOOL_DEFAULT resources first (DXVK refuses to
 * reset while any are alive).
 */

#include "visual/D3D9Hooks.hpp"

#if ENABLE_IMGUI_DRAWING

#include "visual/d3d9_min.hpp"
#include "visual/overlay.hpp"
#include "visual/imgui/imgui_impl_dx9.h"
#include "core/logging.hpp"
#include "core/resolve.hpp"

#include <cstring>
#include <link.h>
#include <string_view>
#include <vector>

namespace d3d9_hooks
{
namespace
{
constexpr const char *DXVK_MODULE      = "libdxvk_d3d9.so";
constexpr const char *DEVICE_TYPE_NAME = "N4dxvk12D3D9DeviceExE";

using present_t    = dx9::hresult (*)(void *, const void *, const void *, void *, const void *);
using present_ex_t = dx9::hresult (*)(void *, const void *, const void *, void *, const void *, std::uint32_t);
using reset_t      = dx9::hresult (*)(void *, void *);
using reset_ex_t   = dx9::hresult (*)(void *, void *, void *);

void **vtable{ nullptr };
present_t original_present{ nullptr };
present_ex_t original_present_ex{ nullptr };
reset_t original_reset{ nullptr };
reset_ex_t original_reset_ex{ nullptr };

struct segment
{
    std::uintptr_t start;
    std::size_t size;
    bool executable;
};

std::vector<segment> dxvk_segments()
{
    std::vector<segment> segments;
    dl_iterate_phdr(
        [](dl_phdr_info *info, std::size_t, void *out) -> int {
            if (!info->dlpi_name || !std::strstr(info->dlpi_name, DXVK_MODULE))
                return 0;
            auto *list = static_cast<std::vector<segment> *>(out);
            for (int i = 0; i < info->dlpi_phnum; ++i)
            {
                const auto &ph = info->dlpi_phdr[i];
                if (ph.p_type == PT_LOAD && (ph.p_flags & PF_R))
                    list->push_back({ info->dlpi_addr + ph.p_vaddr, std::size_t(ph.p_memsz), (ph.p_flags & PF_X) != 0 });
            }
            return 1;
        },
        &segments);
    return segments;
}

bool in_code(const std::vector<segment> &segments, const void *address)
{
    const auto value = reinterpret_cast<std::uintptr_t>(address);
    for (const auto &seg : segments)
        if (seg.executable && value >= seg.start && value < seg.start + seg.size)
            return true;
    return false;
}

// Every 8-byte aligned location in the module's data holding `value`
std::vector<std::uintptr_t> find_pointer(const std::vector<segment> &segments, std::uintptr_t value)
{
    std::vector<std::uintptr_t> hits;
    for (const auto &seg : segments)
    {
        if (seg.executable)
            continue;
        const std::uintptr_t first = (seg.start + 7) & ~std::uintptr_t(7);
        for (std::uintptr_t at = first; at + 8 <= seg.start + seg.size; at += 8)
            if (*reinterpret_cast<const std::uintptr_t *>(at) == value)
                hits.push_back(at);
    }
    return hits;
}

// Itanium ABI: vtable = [offset_to_top][typeinfo*][methods...],
// typeinfo = [vptr][const char *name]
void **find_device_vtable(const std::vector<segment> &segments)
{
    const std::string_view needle{ DEVICE_TYPE_NAME, std::strlen(DEVICE_TYPE_NAME) + 1 };
    for (const auto &seg : segments)
    {
        if (seg.executable)
            continue;
        const std::string_view haystack{ reinterpret_cast<const char *>(seg.start), seg.size };
        for (std::size_t pos = haystack.find(needle); pos != std::string_view::npos; pos = haystack.find(needle, pos + 1))
        {
            const std::uintptr_t name = seg.start + pos;
            for (std::uintptr_t name_ref : find_pointer(segments, name))
            {
                const std::uintptr_t typeinfo = name_ref - 8;
                for (std::uintptr_t ti_ref : find_pointer(segments, typeinfo))
                {
                    if (*reinterpret_cast<const std::intptr_t *>(ti_ref - 8) != 0)
                        continue; // secondary vtable
                    auto **methods = reinterpret_cast<void **>(ti_ref + 8);
                    if (in_code(segments, methods[dx9::device::PRESENT]) && in_code(segments, methods[dx9::device::PRESENT_EX]) && in_code(segments, methods[dx9::device::RESET]) && in_code(segments, methods[dx9::device::RESET_EX]))
                        return methods;
                }
            }
        }
    }
    return nullptr;
}

void draw_overlay(void *device)
{
    static thread_local bool drawing = false;
    if (drawing)
        return;
    drawing = true;

    static void *window_device = nullptr;
    static SDL_Window *window  = nullptr;
    if (device != window_device)
    {
        // DXVK native uses the SDL_Window as the HWND
        dx9::creation_parameters params{};
        if (dx9::succeeded(dx9::vcall(device, dx9::device::GET_CREATION_PARAMETERS, &params)))
            window = static_cast<SDL_Window *>(params.focus_window);
        window_device = device;
        logging::Info("DXVK overlay: device %p window %p", device, window);
    }
    if (window)
    {
        ImGui_ImplDX9_SetDevice(device);
        overlay::frame(window, overlay::Api::DX9);
    }
    drawing = false;
}

dx9::hresult hooked_present(void *device, const void *source, const void *dest, void *window, const void *dirty)
{
    draw_overlay(device);
    return original_present(device, source, dest, window, dirty);
}

dx9::hresult hooked_present_ex(void *device, const void *source, const void *dest, void *window, const void *dirty, std::uint32_t flags)
{
    draw_overlay(device);
    return original_present_ex(device, source, dest, window, dirty, flags);
}

dx9::hresult hooked_reset(void *device, void *params)
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    return original_reset(device, params);
}

dx9::hresult hooked_reset_ex(void *device, void *params, void *mode)
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    return original_reset_ex(device, params, mode);
}

template <typename T>
void patch(std::size_t index, T hook, T &original)
{
    original = reinterpret_cast<T>(vtable[index]);
    if (!cathook::core::memory::write_protected_pointer(&vtable[index], reinterpret_cast<void *>(hook)))
        logging::Info("DXVK overlay: failed to patch vtable slot %zu", index);
}

template <typename T>
void unpatch(std::size_t index, T &original)
{
    if (original)
        cathook::core::memory::write_protected_pointer(&vtable[index], reinterpret_cast<void *>(original));
    original = nullptr;
}
} // namespace

void apply()
{
    const auto segments = dxvk_segments();
    if (segments.empty())
        return; // OpenGL renderer
    vtable = find_device_vtable(segments);
    if (!vtable)
    {
        logging::Info("DXVK overlay: %s is loaded but its D3D9 device vtable was not found — visuals disabled under Vulkan", DXVK_MODULE);
        return;
    }
    logging::Info("DXVK overlay: device vtable at %p", vtable);
    // Reset first so resources are released before any Present-created ones exist
    patch(dx9::device::RESET, hooked_reset, original_reset);
    patch(dx9::device::RESET_EX, hooked_reset_ex, original_reset_ex);
    patch(dx9::device::PRESENT, hooked_present, original_present);
    patch(dx9::device::PRESENT_EX, hooked_present_ex, original_present_ex);
}

void clean()
{
    if (!vtable)
        return;
    unpatch(dx9::device::PRESENT, original_present);
    unpatch(dx9::device::PRESENT_EX, original_present_ex);
    unpatch(dx9::device::RESET, original_reset);
    unpatch(dx9::device::RESET_EX, original_reset_ex);
    vtable = nullptr;
}
} // namespace d3d9_hooks

#else

namespace d3d9_hooks
{
void apply()
{
}
void clean()
{
}
} // namespace d3d9_hooks

#endif
