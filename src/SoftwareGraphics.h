#ifndef ANDROIDIMGUI_SOFTWAREGRAPHICS_H
#define ANDROIDIMGUI_SOFTWAREGRAPHICS_H

#include "AndroidImgui.h"
#include <cstdint>
#include <vector>
#include "imgui.h"

class SoftwareGraphics : public AndroidImgui {
public:
    struct SoftwareTextureData : BaseTexData {
        std::vector<uint32_t> Pixels; // RGBA8888 CPU-side pixel data
        int TexWidth = 0;
        int TexHeight = 0;
    };

private:
    std::vector<uint32_t> m_Framebuffer;
    int m_FbWidth = 0;
    int m_FbHeight = 0;

public:
    bool Create() override;
    void Setup() override;
    void PrepareFrame(bool resize) override;
    void Render(ImDrawData *drawData) override;
    void PrepareShutdown() override;
    void Cleanup() override;
    BaseTexData *LoadTexture(BaseTexData *tex_data, void *pixel_data) override;
    void RemoveTexture(BaseTexData *tex_data) override;

private:
    void SoftwareUpdateTexture(ImTextureData *tex);
    void SoftwareDestroyTexture(ImTextureData *tex);

    void RenderTriangle(
        const ImVec2 &p0, const ImVec2 &p1, const ImVec2 &p2,
        const ImVec2 &uv0, const ImVec2 &uv1, const ImVec2 &uv2,
        ImU32 col0, ImU32 col1, ImU32 col2,
        const SoftwareTextureData *tex,
        const ImVec4 &clipRect);

    static uint32_t BlendPixel(uint32_t dst, uint32_t src);
    static uint32_t SampleTexture(const SoftwareTextureData *tex, float u, float v);
    static uint32_t MultiplyColor(uint32_t texel, uint32_t vertColor);
};

#endif // ANDROIDIMGUI_SOFTWAREGRAPHICS_H

