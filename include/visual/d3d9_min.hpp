/*
 * d3d9_min.hpp
 *
 * Just enough of the Direct3D 9 interface to draw the ImGui overlay through DXVK
 * (TF2's -vulkan renderer). Methods are called by vtable index; DXVK native uses
 * the SysV ABI with `this` as the first argument. Indices were checked against
 * N4dxvk12D3D9DeviceExE in libdxvk_d3d9.so (DXVK 2.3).
 *
 * The real d3d9.h is not used because its Windows typedefs clash with the
 * Source SDK ones pulled in by common.hpp.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace dx9
{

using hresult = std::int32_t;

constexpr bool succeeded(hresult hr)
{
    return hr >= 0;
}

// Formats, pools, usage
constexpr std::uint32_t FMT_A8R8G8B8 = 21;
constexpr std::uint32_t FMT_INDEX16  = 101;
constexpr std::uint32_t FMT_INDEX32  = 102;

constexpr std::uint32_t POOL_DEFAULT = 0;

constexpr std::uint32_t USAGE_WRITEONLY = 0x8;
constexpr std::uint32_t USAGE_DYNAMIC   = 0x200;

constexpr std::uint32_t LOCK_DISCARD = 0x2000;

// Vertex format
constexpr std::uint32_t FVF_XYZ     = 0x002;
constexpr std::uint32_t FVF_DIFFUSE = 0x040;
constexpr std::uint32_t FVF_TEX1    = 0x100;

constexpr std::uint32_t PT_TRIANGLELIST      = 4;
constexpr std::uint32_t BACKBUFFER_TYPE_MONO = 0;
constexpr std::uint32_t SBT_ALL              = 1;

// Render states
constexpr std::uint32_t RS_ZENABLE                  = 7;
constexpr std::uint32_t RS_FILLMODE                 = 8;
constexpr std::uint32_t RS_SHADEMODE                = 9;
constexpr std::uint32_t RS_ZWRITEENABLE             = 14;
constexpr std::uint32_t RS_ALPHATESTENABLE          = 15;
constexpr std::uint32_t RS_SRCBLEND                 = 19;
constexpr std::uint32_t RS_DESTBLEND                = 20;
constexpr std::uint32_t RS_CULLMODE                 = 22;
constexpr std::uint32_t RS_ALPHABLENDENABLE         = 27;
constexpr std::uint32_t RS_FOGENABLE                = 28;
constexpr std::uint32_t RS_SPECULARENABLE           = 29;
constexpr std::uint32_t RS_RANGEFOGENABLE           = 48;
constexpr std::uint32_t RS_STENCILENABLE            = 52;
constexpr std::uint32_t RS_CLIPPING                 = 136;
constexpr std::uint32_t RS_LIGHTING                 = 137;
constexpr std::uint32_t RS_CLIPPLANEENABLE          = 152;
constexpr std::uint32_t RS_COLORWRITEENABLE         = 168;
constexpr std::uint32_t RS_BLENDOP                  = 171;
constexpr std::uint32_t RS_SCISSORTESTENABLE        = 174;
constexpr std::uint32_t RS_SRGBWRITEENABLE          = 194;
constexpr std::uint32_t RS_SEPARATEALPHABLENDENABLE = 206;
constexpr std::uint32_t RS_SRCBLENDALPHA            = 207;
constexpr std::uint32_t RS_DESTBLENDALPHA           = 208;

constexpr std::uint32_t FILL_SOLID      = 3;
constexpr std::uint32_t SHADE_GOURAUD   = 2;
constexpr std::uint32_t CULL_NONE       = 1;
constexpr std::uint32_t BLENDOP_ADD     = 1;
constexpr std::uint32_t BLEND_ONE       = 2;
constexpr std::uint32_t BLEND_SRCALPHA  = 5;
constexpr std::uint32_t BLEND_INVSRCALPHA = 6;

// Texture stage states
constexpr std::uint32_t TSS_COLOROP               = 1;
constexpr std::uint32_t TSS_COLORARG1             = 2;
constexpr std::uint32_t TSS_COLORARG2             = 3;
constexpr std::uint32_t TSS_ALPHAOP               = 4;
constexpr std::uint32_t TSS_ALPHAARG1             = 5;
constexpr std::uint32_t TSS_ALPHAARG2             = 6;
constexpr std::uint32_t TSS_TEXCOORDINDEX         = 11;
constexpr std::uint32_t TSS_TEXTURETRANSFORMFLAGS = 24;

constexpr std::uint32_t TOP_DISABLE    = 1;
constexpr std::uint32_t TOP_SELECTARG2 = 3;
constexpr std::uint32_t TOP_MODULATE   = 4;
constexpr std::uint32_t TA_DIFFUSE     = 0;
constexpr std::uint32_t TA_TEXTURE     = 2;

// Sampler states
constexpr std::uint32_t SAMP_ADDRESSU    = 1;
constexpr std::uint32_t SAMP_ADDRESSV    = 2;
constexpr std::uint32_t SAMP_MAGFILTER   = 5;
constexpr std::uint32_t SAMP_MINFILTER   = 6;
constexpr std::uint32_t SAMP_MIPFILTER   = 7;
constexpr std::uint32_t SAMP_SRGBTEXTURE = 11;

constexpr std::uint32_t TEXF_NONE    = 0;
constexpr std::uint32_t TEXF_POINT   = 1;
constexpr std::uint32_t TEXF_LINEAR  = 2;
constexpr std::uint32_t TADDRESS_CLAMP = 3;

// Transforms
constexpr std::uint32_t TS_VIEW       = 2;
constexpr std::uint32_t TS_PROJECTION = 3;
constexpr std::uint32_t TS_WORLD      = 256;

struct creation_parameters
{
    std::uint32_t adapter_ordinal;
    std::uint32_t device_type;
    void *focus_window; // SDL_Window * under DXVK native
    std::uint32_t behavior_flags;
};

struct surface_desc
{
    std::uint32_t format;
    std::uint32_t type;
    std::uint32_t usage;
    std::uint32_t pool;
    std::uint32_t multisample_type;
    std::uint32_t multisample_quality;
    std::uint32_t width;
    std::uint32_t height;
};

struct locked_rect
{
    std::int32_t pitch;
    void *bits;
};

struct rect
{
    std::int32_t left;
    std::int32_t top;
    std::int32_t right;
    std::int32_t bottom;
};

struct viewport
{
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t width;
    std::uint32_t height;
    float min_z;
    float max_z;
};

struct matrix
{
    float m[4][4];
};

template <typename R = hresult, typename... Args>
inline R vcall(void *object, std::size_t index, Args... args)
{
    using fn_t = R (*)(void *, Args...);
    return (*reinterpret_cast<fn_t **>(object))[index](object, args...);
}

// IUnknown
inline std::uint32_t release(void *object)
{
    return object ? vcall<std::uint32_t>(object, 2) : 0;
}

namespace device
{
// IDirect3DDevice9 / IDirect3DDevice9Ex vtable slots
constexpr std::size_t GET_CREATION_PARAMETERS = 9;
constexpr std::size_t RESET                   = 16;
constexpr std::size_t PRESENT                 = 17;
constexpr std::size_t GET_BACK_BUFFER         = 18;
constexpr std::size_t CREATE_TEXTURE          = 23;
constexpr std::size_t CREATE_VERTEX_BUFFER    = 26;
constexpr std::size_t CREATE_INDEX_BUFFER     = 27;
constexpr std::size_t SET_RENDER_TARGET       = 37;
constexpr std::size_t GET_RENDER_TARGET       = 38;
constexpr std::size_t SET_DEPTH_STENCIL       = 39;
constexpr std::size_t GET_DEPTH_STENCIL       = 40;
constexpr std::size_t BEGIN_SCENE             = 41;
constexpr std::size_t END_SCENE               = 42;
constexpr std::size_t SET_TRANSFORM           = 44;
constexpr std::size_t SET_VIEWPORT            = 47;
constexpr std::size_t SET_RENDER_STATE        = 57;
constexpr std::size_t CREATE_STATE_BLOCK      = 59;
constexpr std::size_t SET_TEXTURE             = 65;
constexpr std::size_t SET_TEXTURE_STAGE_STATE = 67;
constexpr std::size_t SET_SAMPLER_STATE       = 69;
constexpr std::size_t SET_SCISSOR_RECT        = 75;
constexpr std::size_t DRAW_INDEXED_PRIMITIVE  = 82;
constexpr std::size_t SET_FVF                 = 89;
constexpr std::size_t SET_VERTEX_SHADER       = 92;
constexpr std::size_t SET_STREAM_SOURCE       = 100;
constexpr std::size_t SET_INDICES             = 104;
constexpr std::size_t SET_PIXEL_SHADER        = 107;
constexpr std::size_t PRESENT_EX              = 121;
constexpr std::size_t RESET_EX                = 132;
} // namespace device

namespace surface
{
constexpr std::size_t GET_DESC = 12;
}

namespace texture
{
constexpr std::size_t LOCK_RECT   = 19;
constexpr std::size_t UNLOCK_RECT = 20;
} // namespace texture

namespace buffer
{
// Same slots for vertex and index buffers
constexpr std::size_t LOCK   = 11;
constexpr std::size_t UNLOCK = 12;
} // namespace buffer

namespace state_block
{
constexpr std::size_t APPLY = 5;
}

} // namespace dx9
