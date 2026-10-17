/*
 * imgui_impl_dx9.cpp
 *
 * Port of the Dear ImGui DX9 renderer, talking to DXVK through d3d9_min.hpp.
 */

#include "visual/imgui/imgui_impl_dx9.h"
#include "visual/d3d9_min.hpp"
#include "core/logging.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

namespace
{
struct custom_vertex
{
    float pos[3];
    std::uint32_t col;
    float uv[2];
};
constexpr std::uint32_t CUSTOM_FVF = dx9::FVF_XYZ | dx9::FVF_DIFFUSE | dx9::FVF_TEX1;

// ImTextureID points at one of these so the id survives device resets;
// the D3D texture is (re)created from the kept pixels on first use.
struct dx9_texture
{
    void *texture{ nullptr };
    std::vector<unsigned char> rgba;
    int width{ 0 };
    int height{ 0 };
    bool linear{ true };
};

void *g_device        = nullptr;
void *g_vertex_buffer = nullptr;
void *g_index_buffer  = nullptr;
int g_vertex_capacity = 0;
int g_index_capacity  = 0;
int g_backbuffer_w    = 0;
int g_backbuffer_h    = 0;
std::vector<dx9_texture *> g_textures;

void release_buffers()
{
    dx9::release(g_vertex_buffer);
    dx9::release(g_index_buffer);
    g_vertex_buffer   = nullptr;
    g_index_buffer    = nullptr;
    g_vertex_capacity = 0;
    g_index_capacity  = 0;
}

bool upload(dx9_texture *tex)
{
    if (!g_device || tex->rgba.empty())
        return false;
    void *created = nullptr;
    if (!dx9::succeeded(dx9::vcall(g_device, dx9::device::CREATE_TEXTURE, std::uint32_t(tex->width), std::uint32_t(tex->height), std::uint32_t(1), dx9::USAGE_DYNAMIC, dx9::FMT_A8R8G8B8, dx9::POOL_DEFAULT, &created, (void *) nullptr)) || !created)
    {
        logging::Info("dx9: CreateTexture %dx%d failed", tex->width, tex->height);
        return false;
    }
    dx9::locked_rect locked{};
    if (!dx9::succeeded(dx9::vcall(created, dx9::texture::LOCK_RECT, std::uint32_t(0), &locked, (const dx9::rect *) nullptr, dx9::LOCK_DISCARD)))
    {
        dx9::release(created);
        return false;
    }
    // RGBA bytes -> A8R8G8B8 texels
    for (int y = 0; y < tex->height; ++y)
    {
        const unsigned char *src = tex->rgba.data() + std::size_t(y) * tex->width * 4;
        auto *dst                = reinterpret_cast<std::uint32_t *>(static_cast<unsigned char *>(locked.bits) + std::size_t(y) * locked.pitch);
        for (int x = 0; x < tex->width; ++x, src += 4)
            dst[x] = (std::uint32_t(src[3]) << 24) | (std::uint32_t(src[0]) << 16) | (std::uint32_t(src[1]) << 8) | std::uint32_t(src[2]);
    }
    dx9::vcall(created, dx9::texture::UNLOCK_RECT, std::uint32_t(0));
    tex->texture = created;
    return true;
}

bool ensure_buffers(int vertex_count, int index_count)
{
    if (!g_vertex_buffer || g_vertex_capacity < vertex_count)
    {
        dx9::release(g_vertex_buffer);
        g_vertex_buffer   = nullptr;
        g_vertex_capacity = vertex_count + 5000;
        if (!dx9::succeeded(dx9::vcall(g_device, dx9::device::CREATE_VERTEX_BUFFER, std::uint32_t(g_vertex_capacity * sizeof(custom_vertex)), dx9::USAGE_DYNAMIC | dx9::USAGE_WRITEONLY, CUSTOM_FVF, dx9::POOL_DEFAULT, &g_vertex_buffer, (void *) nullptr)))
        {
            g_vertex_buffer = nullptr;
            return false;
        }
    }
    if (!g_index_buffer || g_index_capacity < index_count)
    {
        dx9::release(g_index_buffer);
        g_index_buffer   = nullptr;
        g_index_capacity = index_count + 10000;
        const std::uint32_t format = sizeof(ImDrawIdx) == 2 ? dx9::FMT_INDEX16 : dx9::FMT_INDEX32;
        if (!dx9::succeeded(dx9::vcall(g_device, dx9::device::CREATE_INDEX_BUFFER, std::uint32_t(g_index_capacity * sizeof(ImDrawIdx)), dx9::USAGE_DYNAMIC | dx9::USAGE_WRITEONLY, format, dx9::POOL_DEFAULT, &g_index_buffer, (void *) nullptr)))
        {
            g_index_buffer = nullptr;
            return false;
        }
    }
    return true;
}

void set_render_state(std::uint32_t state, std::uint32_t value)
{
    dx9::vcall(g_device, dx9::device::SET_RENDER_STATE, state, value);
}

void set_stage_state(std::uint32_t stage, std::uint32_t state, std::uint32_t value)
{
    dx9::vcall(g_device, dx9::device::SET_TEXTURE_STAGE_STATE, stage, state, value);
}

void set_sampler_state(std::uint32_t state, std::uint32_t value)
{
    dx9::vcall(g_device, dx9::device::SET_SAMPLER_STATE, std::uint32_t(0), state, value);
}

void setup_render_state(ImDrawData *draw_data, int fb_width, int fb_height)
{
    dx9::viewport vp{ 0, 0, std::uint32_t(fb_width), std::uint32_t(fb_height), 0.0f, 1.0f };
    dx9::vcall(g_device, dx9::device::SET_VIEWPORT, &vp);

    dx9::vcall(g_device, dx9::device::SET_PIXEL_SHADER, (void *) nullptr);
    dx9::vcall(g_device, dx9::device::SET_VERTEX_SHADER, (void *) nullptr);
    dx9::vcall(g_device, dx9::device::SET_STREAM_SOURCE, std::uint32_t(0), g_vertex_buffer, std::uint32_t(0), std::uint32_t(sizeof(custom_vertex)));
    dx9::vcall(g_device, dx9::device::SET_INDICES, g_index_buffer);
    dx9::vcall(g_device, dx9::device::SET_FVF, CUSTOM_FVF);

    set_render_state(dx9::RS_FILLMODE, dx9::FILL_SOLID);
    set_render_state(dx9::RS_SHADEMODE, dx9::SHADE_GOURAUD);
    set_render_state(dx9::RS_ZWRITEENABLE, 0);
    set_render_state(dx9::RS_ALPHATESTENABLE, 0);
    set_render_state(dx9::RS_CULLMODE, dx9::CULL_NONE);
    set_render_state(dx9::RS_ZENABLE, 0);
    set_render_state(dx9::RS_ALPHABLENDENABLE, 1);
    set_render_state(dx9::RS_BLENDOP, dx9::BLENDOP_ADD);
    set_render_state(dx9::RS_SRCBLEND, dx9::BLEND_SRCALPHA);
    set_render_state(dx9::RS_DESTBLEND, dx9::BLEND_INVSRCALPHA);
    set_render_state(dx9::RS_SEPARATEALPHABLENDENABLE, 1);
    set_render_state(dx9::RS_SRCBLENDALPHA, dx9::BLEND_ONE);
    set_render_state(dx9::RS_DESTBLENDALPHA, dx9::BLEND_INVSRCALPHA);
    set_render_state(dx9::RS_SCISSORTESTENABLE, 1);
    set_render_state(dx9::RS_FOGENABLE, 0);
    set_render_state(dx9::RS_RANGEFOGENABLE, 0);
    set_render_state(dx9::RS_SPECULARENABLE, 0);
    set_render_state(dx9::RS_STENCILENABLE, 0);
    set_render_state(dx9::RS_CLIPPING, 1);
    set_render_state(dx9::RS_LIGHTING, 0);
    set_render_state(dx9::RS_CLIPPLANEENABLE, 0);
    set_render_state(dx9::RS_COLORWRITEENABLE, 0xF);
    // Same as disabling GL_FRAMEBUFFER_SRGB in the GL renderer
    set_render_state(dx9::RS_SRGBWRITEENABLE, 0);

    set_stage_state(0, dx9::TSS_COLORARG1, dx9::TA_TEXTURE);
    set_stage_state(0, dx9::TSS_COLORARG2, dx9::TA_DIFFUSE);
    set_stage_state(0, dx9::TSS_ALPHAARG1, dx9::TA_TEXTURE);
    set_stage_state(0, dx9::TSS_ALPHAARG2, dx9::TA_DIFFUSE);
    set_stage_state(0, dx9::TSS_TEXCOORDINDEX, 0);
    set_stage_state(0, dx9::TSS_TEXTURETRANSFORMFLAGS, 0);
    set_stage_state(1, dx9::TSS_COLOROP, dx9::TOP_DISABLE);
    set_stage_state(1, dx9::TSS_ALPHAOP, dx9::TOP_DISABLE);

    set_sampler_state(dx9::SAMP_ADDRESSU, dx9::TADDRESS_CLAMP);
    set_sampler_state(dx9::SAMP_ADDRESSV, dx9::TADDRESS_CLAMP);
    set_sampler_state(dx9::SAMP_MIPFILTER, dx9::TEXF_NONE);
    set_sampler_state(dx9::SAMP_SRGBTEXTURE, 0);

    // Orthographic projection with the D3D9 half-pixel offset
    const float L = draw_data->DisplayPos.x + 0.5f;
    const float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x + 0.5f;
    const float T = draw_data->DisplayPos.y + 0.5f;
    const float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y + 0.5f;
    const dx9::matrix identity{ { { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } } };
    const dx9::matrix projection{ { { 2.0f / (R - L), 0.0f, 0.0f, 0.0f }, { 0.0f, 2.0f / (T - B), 0.0f, 0.0f }, { 0.0f, 0.0f, 0.5f, 0.0f }, { (L + R) / (L - R), (T + B) / (B - T), 0.5f, 1.0f } } };
    dx9::vcall(g_device, dx9::device::SET_TRANSFORM, dx9::TS_WORLD, &identity);
    dx9::vcall(g_device, dx9::device::SET_TRANSFORM, dx9::TS_VIEW, &identity);
    dx9::vcall(g_device, dx9::device::SET_TRANSFORM, dx9::TS_PROJECTION, &projection);
}

void bind_texture(ImTextureID id)
{
    auto *tex = static_cast<dx9_texture *>(id);
    if (tex && !tex->texture)
        upload(tex);
    if (!tex || !tex->texture)
    {
        // Untextured draw lists (ESP buffers) use the vertex colour only
        dx9::vcall(g_device, dx9::device::SET_TEXTURE, std::uint32_t(0), (void *) nullptr);
        set_stage_state(0, dx9::TSS_COLOROP, dx9::TOP_SELECTARG2);
        set_stage_state(0, dx9::TSS_ALPHAOP, dx9::TOP_SELECTARG2);
        return;
    }
    dx9::vcall(g_device, dx9::device::SET_TEXTURE, std::uint32_t(0), tex->texture);
    set_stage_state(0, dx9::TSS_COLOROP, dx9::TOP_MODULATE);
    set_stage_state(0, dx9::TSS_ALPHAOP, dx9::TOP_MODULATE);
    const std::uint32_t filter = tex->linear ? dx9::TEXF_LINEAR : dx9::TEXF_POINT;
    set_sampler_state(dx9::SAMP_MINFILTER, filter);
    set_sampler_state(dx9::SAMP_MAGFILTER, filter);
}
} // namespace

void ImGui_ImplDX9_SetDevice(void *device)
{
    if (device == g_device)
        return;
    // A new device: everything owned by the old one has to go
    ImGui_ImplDX9_InvalidateDeviceObjects();
    g_device = device;
    logging::Info("dx9: overlay device %p", device);
}

void ImGui_ImplDX9_NewFrame()
{
    if (!g_device)
        return;
    void *backbuffer = nullptr;
    if (!dx9::succeeded(dx9::vcall(g_device, dx9::device::GET_BACK_BUFFER, std::uint32_t(0), std::uint32_t(0), dx9::BACKBUFFER_TYPE_MONO, &backbuffer)) || !backbuffer)
        return;
    dx9::surface_desc desc{};
    if (dx9::succeeded(dx9::vcall(backbuffer, dx9::surface::GET_DESC, &desc)))
    {
        g_backbuffer_w = int(desc.width);
        g_backbuffer_h = int(desc.height);
    }
    dx9::release(backbuffer);
}

void ImGui_ImplDX9_GetBackBufferSize(int *width, int *height)
{
    if (width)
        *width = g_backbuffer_w;
    if (height)
        *height = g_backbuffer_h;
}

void ImGui_ImplDX9_Render(ImDrawData *draw_data)
{
    if (!g_device || !draw_data || draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
        return;
    const ImVec2 clip_scale = draw_data->FramebufferScale;
    const int fb_width      = int(draw_data->DisplaySize.x * clip_scale.x);
    const int fb_height     = int(draw_data->DisplaySize.y * clip_scale.y);
    if (fb_width <= 0 || fb_height <= 0)
        return;

    // imrenderer appends its own draw lists after ImGui::Render, so the
    // Total*Count fields are stale; count them here.
    int vertex_count = 0, index_count = 0;
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        vertex_count += draw_data->CmdLists[n]->VtxBuffer.Size;
        index_count += draw_data->CmdLists[n]->IdxBuffer.Size;
    }
    if (vertex_count == 0 || !ensure_buffers(vertex_count, index_count))
        return;

    custom_vertex *vtx_dst = nullptr;
    ImDrawIdx *idx_dst     = nullptr;
    if (!dx9::succeeded(dx9::vcall(g_vertex_buffer, dx9::buffer::LOCK, std::uint32_t(0), std::uint32_t(vertex_count * sizeof(custom_vertex)), (void **) &vtx_dst, dx9::LOCK_DISCARD)))
        return;
    if (!dx9::succeeded(dx9::vcall(g_index_buffer, dx9::buffer::LOCK, std::uint32_t(0), std::uint32_t(index_count * sizeof(ImDrawIdx)), (void **) &idx_dst, dx9::LOCK_DISCARD)))
    {
        dx9::vcall(g_vertex_buffer, dx9::buffer::UNLOCK);
        return;
    }
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList *cmd_list = draw_data->CmdLists[n];
        const ImDrawVert *vtx_src  = cmd_list->VtxBuffer.Data;
        for (int i = 0; i < cmd_list->VtxBuffer.Size; i++, vtx_src++, vtx_dst++)
        {
            vtx_dst->pos[0] = vtx_src->pos.x;
            vtx_dst->pos[1] = vtx_src->pos.y;
            vtx_dst->pos[2] = 0.0f;
            // ImGui packs ABGR, D3DCOLOR is ARGB
            vtx_dst->col   = (vtx_src->col & 0xFF00FF00) | ((vtx_src->col & 0xFF0000) >> 16) | ((vtx_src->col & 0xFF) << 16);
            vtx_dst->uv[0] = vtx_src->uv.x;
            vtx_dst->uv[1] = vtx_src->uv.y;
        }
        std::memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        idx_dst += cmd_list->IdxBuffer.Size;
    }
    dx9::vcall(g_vertex_buffer, dx9::buffer::UNLOCK);
    dx9::vcall(g_index_buffer, dx9::buffer::UNLOCK);

    // Save the game's state; render targets are not part of a state block
    void *state_block = nullptr;
    if (!dx9::succeeded(dx9::vcall(g_device, dx9::device::CREATE_STATE_BLOCK, dx9::SBT_ALL, &state_block)) || !state_block)
        return;
    void *old_target = nullptr, *old_depth = nullptr, *backbuffer = nullptr;
    dx9::vcall(g_device, dx9::device::GET_RENDER_TARGET, std::uint32_t(0), &old_target);
    dx9::vcall(g_device, dx9::device::GET_DEPTH_STENCIL, &old_depth);
    if (dx9::succeeded(dx9::vcall(g_device, dx9::device::GET_BACK_BUFFER, std::uint32_t(0), std::uint32_t(0), dx9::BACKBUFFER_TYPE_MONO, &backbuffer)) && backbuffer)
        dx9::vcall(g_device, dx9::device::SET_RENDER_TARGET, std::uint32_t(0), backbuffer);
    dx9::vcall(g_device, dx9::device::SET_DEPTH_STENCIL, (void *) nullptr);

    // Present comes after the game's EndScene, so we open our own scene
    const bool own_scene = dx9::succeeded(dx9::vcall(g_device, dx9::device::BEGIN_SCENE));
    setup_render_state(draw_data, fb_width, fb_height);

    const ImVec2 clip_off = draw_data->DisplayPos;
    int global_vtx_offset = 0;
    int global_idx_offset = 0;
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList *cmd_list = draw_data->CmdLists[n];
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd *pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback)
            {
                pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                const dx9::rect scissor{ std::int32_t((pcmd->ClipRect.x - clip_off.x) * clip_scale.x), std::int32_t((pcmd->ClipRect.y - clip_off.y) * clip_scale.y), std::int32_t((pcmd->ClipRect.z - clip_off.x) * clip_scale.x), std::int32_t((pcmd->ClipRect.w - clip_off.y) * clip_scale.y) };
                if (scissor.left < fb_width && scissor.top < fb_height && scissor.right >= 0 && scissor.bottom >= 0 && pcmd->ElemCount > 0)
                {
                    dx9::vcall(g_device, dx9::device::SET_SCISSOR_RECT, &scissor);
                    bind_texture(pcmd->TextureId);
                    dx9::vcall(g_device, dx9::device::DRAW_INDEXED_PRIMITIVE, dx9::PT_TRIANGLELIST, std::int32_t(global_vtx_offset), std::uint32_t(0), std::uint32_t(cmd_list->VtxBuffer.Size), std::uint32_t(global_idx_offset), std::uint32_t(pcmd->ElemCount / 3));
                }
            }
            global_idx_offset += int(pcmd->ElemCount);
        }
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }

    if (own_scene)
        dx9::vcall(g_device, dx9::device::END_SCENE);

    if (old_target)
        dx9::vcall(g_device, dx9::device::SET_RENDER_TARGET, std::uint32_t(0), old_target);
    dx9::vcall(g_device, dx9::device::SET_DEPTH_STENCIL, old_depth);
    dx9::vcall(state_block, dx9::state_block::APPLY);
    dx9::release(state_block);
    dx9::release(backbuffer);
    dx9::release(old_target);
    dx9::release(old_depth);
}

ImTextureID ImGui_ImplDX9_CreateTexture(const unsigned char *rgba, int width, int height, bool linear)
{
    if (!rgba || width <= 0 || height <= 0)
        return nullptr;
    auto *tex   = new dx9_texture();
    tex->width  = width;
    tex->height = height;
    tex->linear = linear;
    tex->rgba.assign(rgba, rgba + std::size_t(width) * height * 4);
    upload(tex);
    g_textures.push_back(tex);
    return tex;
}

void ImGui_ImplDX9_DestroyTexture(ImTextureID id)
{
    auto *tex = static_cast<dx9_texture *>(id);
    if (!tex)
        return;
    g_textures.erase(std::remove(g_textures.begin(), g_textures.end(), tex), g_textures.end());
    dx9::release(tex->texture);
    delete tex;
}

void ImGui_ImplDX9_InvalidateDeviceObjects()
{
    release_buffers();
    for (auto *tex : g_textures)
    {
        dx9::release(tex->texture);
        tex->texture = nullptr;
    }
}
