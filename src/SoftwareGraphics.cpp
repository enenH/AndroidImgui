#include <android/native_window.h>
#include <android/log.h>
#include <cstring>
#include <cmath>
#include <algorithm>
#include "SoftwareGraphics.h"
#include "imgui.h"

#define SW_LOG_TAG "SoftwareGraphics"
#define SW_LOGI(...) __android_log_print(ANDROID_LOG_INFO, SW_LOG_TAG, __VA_ARGS__)
#define SW_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, SW_LOG_TAG, __VA_ARGS__)

// --- Helpers ---

static inline int iminVal(int a, int b) { return a < b ? a : b; }
static inline int imaxVal(int a, int b) { return a > b ? a : b; }
static inline float fclamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Fast integer division by 255: exact for all 0–65535 inputs
// Equivalent to (x + 127) / 255
static inline uint32_t div255(uint32_t x) {
    return (x + 128 + ((x + 128) >> 8)) >> 8;
}

// Extract RGBA components from ImGui packed color (ImU32 is ABGR in memory: 0xAABBGGRR)
static inline void UnpackColor(ImU32 col, uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &a) {
    r = (col >> IM_COL32_R_SHIFT) & 0xFF;
    g = (col >> IM_COL32_G_SHIFT) & 0xFF;
    b = (col >> IM_COL32_B_SHIFT) & 0xFF;
    a = (col >> IM_COL32_A_SHIFT) & 0xFF;
}

// Unpack color to float channels (done once per vertex, not per pixel)
static inline void UnpackColorF(ImU32 col, float &r, float &g, float &b, float &a) {
    r = (float)((col >> IM_COL32_R_SHIFT) & 0xFF);
    g = (float)((col >> IM_COL32_G_SHIFT) & 0xFF);
    b = (float)((col >> IM_COL32_B_SHIFT) & 0xFF);
    a = (float)((col >> IM_COL32_A_SHIFT) & 0xFF);
}

static inline uint32_t PackRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    // Pack into ANativeWindow_Buffer format: RGBA8888 (0xAABBGGRR on little-endian)
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
}

// --- SoftwareGraphics implementation ---

bool SoftwareGraphics::Create() {
    m_FbWidth = (int)m_Width;
    m_FbHeight = (int)m_Height;
    ANativeWindow_setBuffersGeometry(m_Window, m_FbWidth, m_FbHeight, WINDOW_FORMAT_RGBA_8888);
    m_Framebuffer.resize(m_FbWidth * m_FbHeight, 0);
    SW_LOGI("Software renderer created: %dx%d", m_FbWidth, m_FbHeight);
    return true;
}

void SoftwareGraphics::Setup() {
    ImGuiIO &io = ImGui::GetIO();
    io.BackendRendererName = "imgui_impl_software";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    SW_LOGI("Software renderer backend initialized with RendererHasTextures support");
}

void SoftwareGraphics::PrepareFrame(bool resize) {
    if (resize) {
        m_FbWidth = (int)m_Width;
        m_FbHeight = (int)m_Height;
        ANativeWindow_setBuffersGeometry(m_Window, m_FbWidth, m_FbHeight, WINDOW_FORMAT_RGBA_8888);
        m_Framebuffer.resize(m_FbWidth * m_FbHeight);
    }
    // Clear framebuffer to transparent black
    memset(m_Framebuffer.data(), 0, m_Framebuffer.size() * sizeof(uint32_t));
}

void SoftwareGraphics::Render(ImDrawData *drawData) {
    if (!drawData || drawData->CmdListsCount == 0)
        return;

    // Catch up with texture updates (mirrors ImGui_ImplOpenGL3_RenderDrawData pattern)
    if (drawData->Textures != nullptr)
        for (ImTextureData *tex : *drawData->Textures)
            if (tex->Status != ImTextureStatus_OK)
                SoftwareUpdateTexture(tex);

    for (int n = 0; n < drawData->CmdListsCount; n++) {
        const ImDrawList *cmdList = drawData->CmdLists[n];
        const ImDrawVert *vtxBuffer = cmdList->VtxBuffer.Data;
        const ImDrawIdx *idxBuffer = cmdList->IdxBuffer.Data;

        for (int cmdIdx = 0; cmdIdx < cmdList->CmdBuffer.Size; cmdIdx++) {
            const ImDrawCmd &pcmd = cmdList->CmdBuffer[cmdIdx];

            if (pcmd.UserCallback) {
                pcmd.UserCallback(cmdList, &pcmd);
                continue;
            }

            const auto *tex = (const SoftwareTextureData *)(intptr_t)pcmd.GetTexID();
            ImVec4 clipRect = pcmd.ClipRect;

            for (unsigned int i = 0; i < pcmd.ElemCount; i += 3) {
                const ImDrawVert &v0 = vtxBuffer[idxBuffer[pcmd.IdxOffset + i + 0]];
                const ImDrawVert &v1 = vtxBuffer[idxBuffer[pcmd.IdxOffset + i + 1]];
                const ImDrawVert &v2 = vtxBuffer[idxBuffer[pcmd.IdxOffset + i + 2]];

                RenderTriangle(
                    v0.pos, v1.pos, v2.pos,
                    v0.uv, v1.uv, v2.uv,
                    v0.col, v1.col, v2.col,
                    tex, clipRect);
            }
        }
    }

    // Blit framebuffer to ANativeWindow
    ANativeWindow_Buffer buffer;
    if (ANativeWindow_lock(m_Window, &buffer, nullptr) == 0) {
        auto *dst = (uint32_t *)buffer.bits;
        int copyWidth = iminVal(m_FbWidth, buffer.width);
        int copyHeight = iminVal(m_FbHeight, buffer.height);

        for (int y = 0; y < copyHeight; y++) {
            memcpy(dst + y * buffer.stride, m_Framebuffer.data() + y * m_FbWidth, copyWidth * sizeof(uint32_t));
        }
        ANativeWindow_unlockAndPost(m_Window);
    } else {
        SW_LOGE("Failed to lock ANativeWindow");
    }
}

void SoftwareGraphics::PrepareShutdown() {
    // Destroy all backend-managed textures
    ImGuiPlatformIO &platform_io = ImGui::GetPlatformIO();
    for (ImTextureData *tex : platform_io.Textures) {
        SoftwareDestroyTexture(tex);
    }

    ImGuiIO &io = ImGui::GetIO();
    io.BackendRendererName = nullptr;
    io.BackendFlags &= ~ImGuiBackendFlags_RendererHasTextures;
    platform_io.ClearRendererHandlers();
}

void SoftwareGraphics::Cleanup() {
    m_Framebuffer.clear();
    m_Framebuffer.shrink_to_fit();
    m_FbWidth = 0;
    m_FbHeight = 0;
}

BaseTexData *SoftwareGraphics::LoadTexture(BaseTexData *tex, void *pixel_data) {
    auto *texData = new SoftwareTextureData();
    texData->Width = tex->Width;
    texData->Height = tex->Height;
    texData->Channels = tex->Channels;
    texData->TexWidth = tex->Width;
    texData->TexHeight = tex->Height;
    texData->Pixels.resize(tex->Width * tex->Height);
    memcpy(texData->Pixels.data(), pixel_data, tex->Width * tex->Height * 4);
    texData->DS = (void *)texData;
    return texData;
}

void SoftwareGraphics::RemoveTexture(BaseTexData *tex) {
    auto *texData = (SoftwareTextureData *)tex;
    delete texData;
}

// --- Software texture management (mirrors ImGui_ImplOpenGL3_UpdateTexture) ---

void SoftwareGraphics::SoftwareDestroyTexture(ImTextureData *tex) {
    auto *swTex = (SoftwareTextureData *)tex->BackendUserData;
    if (swTex) {
        delete swTex;
        tex->BackendUserData = nullptr;
    }
    tex->SetTexID(ImTextureID_Invalid);
    tex->SetStatus(ImTextureStatus_Destroyed);
}

void SoftwareGraphics::SoftwareUpdateTexture(ImTextureData *tex) {
    if (tex->Status == ImTextureStatus_WantCreate) {
        // Create a new software texture from ImTextureData pixels
        IM_ASSERT(tex->TexID == ImTextureID_Invalid && tex->BackendUserData == nullptr);

        auto *swTex = new SoftwareTextureData();
        swTex->TexWidth = tex->Width;
        swTex->TexHeight = tex->Height;
        swTex->Width = tex->Width;
        swTex->Height = tex->Height;
        swTex->Channels = tex->BytesPerPixel;

        if (tex->Format == ImTextureFormat_RGBA32) {
            swTex->Pixels.resize(tex->Width * tex->Height);
            memcpy(swTex->Pixels.data(), tex->GetPixels(), tex->Width * tex->Height * 4);
        } else if (tex->Format == ImTextureFormat_Alpha8) {
            // Convert Alpha8 to RGBA32
            swTex->Pixels.resize(tex->Width * tex->Height);
            const uint8_t *src = (const uint8_t *)tex->GetPixels();
            for (int i = 0; i < tex->Width * tex->Height; i++) {
                swTex->Pixels[i] = PackRGBA(255, 255, 255, src[i]);
            }
        }

        tex->BackendUserData = swTex;
        tex->SetTexID((ImTextureID)(intptr_t)swTex);
        tex->SetStatus(ImTextureStatus_OK);
        //SW_LOGI("Texture created: %dx%d (id=%d)", tex->Width, tex->Height, tex->UniqueID);
    }
    else if (tex->Status == ImTextureStatus_WantUpdates) {
        // Update changed regions of an existing software texture
        auto *swTex = (SoftwareTextureData *)tex->BackendUserData;
        IM_ASSERT(swTex != nullptr);

        if (tex->Format == ImTextureFormat_RGBA32) {
            for (ImTextureRect &r : tex->Updates) {
                for (int y = 0; y < r.h; y++) {
                    const uint32_t *srcRow = (const uint32_t *)tex->GetPixelsAt(r.x, r.y + y);
                    uint32_t *dstRow = &swTex->Pixels[(r.y + y) * swTex->TexWidth + r.x];
                    memcpy(dstRow, srcRow, r.w * sizeof(uint32_t));
                }
            }
        } else if (tex->Format == ImTextureFormat_Alpha8) {
            for (ImTextureRect &r : tex->Updates) {
                for (int y = 0; y < r.h; y++) {
                    const uint8_t *srcRow = (const uint8_t *)tex->GetPixelsAt(r.x, r.y + y);
                    uint32_t *dstRow = &swTex->Pixels[(r.y + y) * swTex->TexWidth + r.x];
                    for (int x = 0; x < r.w; x++) {
                        dstRow[x] = PackRGBA(255, 255, 255, srcRow[x]);
                    }
                }
            }
        }

        tex->SetStatus(ImTextureStatus_OK);
    }
    else if (tex->Status == ImTextureStatus_WantDestroy && tex->UnusedFrames > 0) {
        SoftwareDestroyTexture(tex);
    }
}

// --- Software triangle rasterization ---

void SoftwareGraphics::RenderTriangle(
    const ImVec2 &p0, const ImVec2 &p1, const ImVec2 &p2,
    const ImVec2 &uv0, const ImVec2 &uv1, const ImVec2 &uv2,
    ImU32 col0, ImU32 col1, ImU32 col2,
    const SoftwareTextureData *tex,
    const ImVec4 &clipRect)
{
    // Compute bounding box
    float minXf = std::min({p0.x, p1.x, p2.x});
    float minYf = std::min({p0.y, p1.y, p2.y});
    float maxXf = std::max({p0.x, p1.x, p2.x});
    float maxYf = std::max({p0.y, p1.y, p2.y});

    // Clip to scissor rect and framebuffer
    int minX = imaxVal((int)minXf, imaxVal((int)clipRect.x, 0));
    int minY = imaxVal((int)minYf, imaxVal((int)clipRect.y, 0));
    int maxX = iminVal((int)maxXf, iminVal((int)clipRect.z - 1, m_FbWidth - 1));
    int maxY = iminVal((int)maxYf, iminVal((int)clipRect.w - 1, m_FbHeight - 1));

    if (minX > maxX || minY > maxY)
        return;

    // Edge function / barycentric setup
    float denom = (p1.y - p2.y) * (p0.x - p2.x) + (p2.x - p1.x) * (p0.y - p2.y);
    if (denom == 0.0f)
        return;
    float invDenom = 1.0f / denom;

    // === Optimization 1: Precompute incremental edge function constants ===
    // Edge function gradients (constant per triangle)
    const float A01 = (p1.y - p2.y) * invDenom; // dw0/dx
    const float B01 = (p2.x - p1.x) * invDenom; // dw0/dy
    const float A12 = (p2.y - p0.y) * invDenom;  // dw1/dx
    const float B12 = (p0.x - p2.x) * invDenom;  // dw1/dy

    // Compute w0, w1 at the starting pixel center (minX+0.5, minY+0.5)
    const float startPx = (float)minX + 0.5f;
    const float startPy = (float)minY + 0.5f;
    const float w0_start = ((p1.y - p2.y) * (startPx - p2.x) + (p2.x - p1.x) * (startPy - p2.y)) * invDenom;
    const float w1_start = ((p2.y - p0.y) * (startPx - p2.x) + (p0.x - p2.x) * (startPy - p2.y)) * invDenom;

    // === Optimization 2: Hoist vertex color unpacking out of inner loop ===
    float cr0, cg0, cb0, ca0;
    float cr1, cg1, cb1, ca1;
    float cr2, cg2, cb2, ca2;
    UnpackColorF(col0, cr0, cg0, cb0, ca0);
    UnpackColorF(col1, cr1, cg1, cb1, ca1);
    UnpackColorF(col2, cr2, cg2, cb2, ca2);

    // === Optimization 3: Precompute texture info once per triangle ===
    const bool hasTex = tex && !tex->Pixels.empty();
    const uint32_t *texPixels = hasTex ? tex->Pixels.data() : nullptr;
    const int texW = hasTex ? tex->TexWidth : 0;
    const int texH = hasTex ? tex->TexHeight : 0;
    const float texWf = (float)(texW - 1);
    const float texHf = (float)(texH - 1);

    // === Optimization 4: Check if all 3 vertex colors are identical (common case: flat color) ===
    const bool flatColor = (col0 == col1 && col1 == col2);

    // Framebuffer pointer
    uint32_t *fb = m_Framebuffer.data();
    const int fbWidth = m_FbWidth;

    float w0_row = w0_start;
    float w1_row = w1_start;

    for (int y = minY; y <= maxY; y++) {
        float w0 = w0_row;
        float w1 = w1_row;
        uint32_t *fbRow = fb + y * fbWidth;

        for (int x = minX; x <= maxX; x++) {
            float w2 = 1.0f - w0 - w1;

            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                // --- Interpolate vertex color (all in integer, no pack/unpack round-trip) ---
                uint32_t vr, vg, vb, va;
                if (flatColor) {
                    // Fast path: all vertices same color, no interpolation needed
                    vr = (uint32_t)cr0;
                    vg = (uint32_t)cg0;
                    vb = (uint32_t)cb0;
                    va = (uint32_t)ca0;
                } else {
                    vr = (uint32_t)fclamp(w0 * cr0 + w1 * cr1 + w2 * cr2, 0.0f, 255.0f);
                    vg = (uint32_t)fclamp(w0 * cg0 + w1 * cg1 + w2 * cg2, 0.0f, 255.0f);
                    vb = (uint32_t)fclamp(w0 * cb0 + w1 * cb1 + w2 * cb2, 0.0f, 255.0f);
                    va = (uint32_t)fclamp(w0 * ca0 + w1 * ca1 + w2 * ca2, 0.0f, 255.0f);
                }

                // --- Sample texture directly into components (no intermediate pack/unpack) ---
                uint32_t tr, tg, tb, ta;
                if (texPixels) {
                    float u = w0 * uv0.x + w1 * uv1.x + w2 * uv2.x;
                    float v = w0 * uv0.y + w1 * uv1.y + w2 * uv2.y;
                    u = fclamp(u, 0.0f, 1.0f);
                    v = fclamp(v, 0.0f, 1.0f);
                    int tx = (int)(u * texWf);
                    int ty = (int)(v * texHf);
                    // Pixels already stored as RGBA8888 — read directly as uint32_t
                    uint32_t texel = texPixels[ty * texW + tx];
                    tr = texel & 0xFF;
                    tg = (texel >> 8) & 0xFF;
                    tb = (texel >> 16) & 0xFF;
                    ta = (texel >> 24) & 0xFF;
                } else {
                    tr = 255; tg = 255; tb = 255; ta = 255;
                }

                // --- Multiply color: src = tex * vert (using fast div255) ---
                uint32_t sr = div255(tr * vr);
                uint32_t sg = div255(tg * vg);
                uint32_t sb = div255(tb * vb);
                uint32_t sa = div255(ta * va);

                // --- Alpha blend onto framebuffer (inlined, no pack/unpack) ---
                if (sa == 0) {
                    // Fully transparent — skip
                } else if (sa == 255) {
                    // Fully opaque — direct write
                    fbRow[x] = ((uint32_t)sa << 24) | ((uint32_t)sb << 16) | ((uint32_t)sg << 8) | (uint32_t)sr;
                } else {
                    uint32_t dst = fbRow[x];
                    uint32_t dr = dst & 0xFF;
                    uint32_t dg = (dst >> 8) & 0xFF;
                    uint32_t db = (dst >> 16) & 0xFF;
                    uint32_t da = (dst >> 24) & 0xFF;

                    uint32_t invSa = 255 - sa;
                    uint32_t outR = div255(sr * sa + dr * invSa);
                    uint32_t outG = div255(sg * sa + dg * invSa);
                    uint32_t outB = div255(sb * sa + db * invSa);
                    uint32_t outA = sa + div255(da * invSa);

                    fbRow[x] = (outA << 24) | (outB << 16) | (outG << 8) | outR;
                }
            }

            // Incremental step: advance w0, w1 by their x-gradients
            w0 += A01;
            w1 += A12;
        }

        // Incremental step: advance row start by y-gradients
        w0_row += B01;
        w1_row += B12;
    }
}

uint32_t SoftwareGraphics::SampleTexture(const SoftwareTextureData *tex, float u, float v) {
    u = fclamp(u, 0.0f, 1.0f);
    v = fclamp(v, 0.0f, 1.0f);
    int tx = (int)(u * (tex->TexWidth - 1));
    int ty = (int)(v * (tex->TexHeight - 1));
    // Pixels already stored as RGBA8888 uint32_t — return directly, no repack needed
    return tex->Pixels[ty * tex->TexWidth + tx];
}

uint32_t SoftwareGraphics::MultiplyColor(uint32_t texel, uint32_t vertColor) {
    uint32_t tr = (texel >> 0) & 0xFF;
    uint32_t tg = (texel >> 8) & 0xFF;
    uint32_t tb = (texel >> 16) & 0xFF;
    uint32_t ta = (texel >> 24) & 0xFF;

    uint32_t vr = (vertColor >> 0) & 0xFF;
    uint32_t vg = (vertColor >> 8) & 0xFF;
    uint32_t vb = (vertColor >> 16) & 0xFF;
    uint32_t va = (vertColor >> 24) & 0xFF;

    uint8_t r = (uint8_t)div255(tr * vr);
    uint8_t g = (uint8_t)div255(tg * vg);
    uint8_t b = (uint8_t)div255(tb * vb);
    uint8_t a = (uint8_t)div255(ta * va);

    return PackRGBA(r, g, b, a);
}

uint32_t SoftwareGraphics::BlendPixel(uint32_t dst, uint32_t src) {
    uint32_t sa = (src >> 24) & 0xFF;

    if (sa == 0) return dst;
    if (sa == 255) return src;

    uint32_t sr = (src >> 0) & 0xFF;
    uint32_t sg = (src >> 8) & 0xFF;
    uint32_t sb = (src >> 16) & 0xFF;

    uint32_t dr = (dst >> 0) & 0xFF;
    uint32_t dg = (dst >> 8) & 0xFF;
    uint32_t db = (dst >> 16) & 0xFF;
    uint32_t da = (dst >> 24) & 0xFF;

    // Standard alpha blending: out = src * srcA + dst * (1 - srcA)
    uint32_t invSa = 255 - sa;
    uint8_t outR = (uint8_t)div255(sr * sa + dr * invSa);
    uint8_t outG = (uint8_t)div255(sg * sa + dg * invSa);
    uint8_t outB = (uint8_t)div255(sb * sa + db * invSa);
    uint8_t outA = (uint8_t)(sa + div255(da * invSa));

    return PackRGBA(outR, outG, outB, outA);
}

