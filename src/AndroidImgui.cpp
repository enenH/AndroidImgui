#include <android/native_window.h>
#include "AndroidImgui.h"
#include "imgui.h"
#include "my_imgui_impl_android.h"
#include "stb_image.h"

bool AndroidImgui::Init(ANativeWindow *window, float width, float height) {
    m_Window = window;
    m_Width = width;
    m_Height = height;

    ANativeWindow_acquire(window);
    Create();

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();

    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = {width, height};
    io.FontGlobalScale = 1.3f;
    // Setup Dear ImGui style
    //ImGui::StyleColorsDark();
    ImGui::StyleColorsLight();
    ImGuiStyle &style = ImGui::GetStyle();
    style.ScaleAllSizes(3);
    style.WindowRounding = 3.f;
    My_ImGui_ImplAndroid_Init(window);

    Setup();
    return true;
}

void AndroidImgui::NewFrame(bool resize) {
    PrepareFrame(resize);
    My_ImGui_ImplAndroid_NewFrame(resize);
    ImGui::NewFrame();
}

void AndroidImgui::EndFrame() {
    ImGui::Render();
    Render(ImGui::GetDrawData());
}

void AndroidImgui::Shutdown() {
    for (auto &texture: m_Textures) {
        RemoveTexture(texture);
    }
    m_Textures.clear();
    PrepareShutdown();
    My_ImGui_ImplAndroid_Shutdown();
    ImGui::DestroyContext();
    Cleanup();
    ANativeWindow_release(m_Window);
}

BaseTexData *AndroidImgui::LoadTextureData(const std::function<unsigned char *(BaseTexData *)> &loadFunc) {
    BaseTexData tex_data{};

    tex_data.Channels = 4;
    unsigned char *image_data = loadFunc(&tex_data);
    if (image_data == nullptr)
        return nullptr;

    auto result = LoadTexture(&tex_data, image_data);

    stbi_image_free(image_data);

    if (result) {
        m_Textures.push_back(result);
    }
    return result;
}

BaseTexData *AndroidImgui::LoadTextureFromFile(const char *filepath) {
    return LoadTextureData([filepath](BaseTexData *tex_data) {
        return stbi_load(filepath, &tex_data->Width, &tex_data->Height, nullptr, tex_data->Channels);
    });
}

BaseTexData *AndroidImgui::LoadTextureFromMemory(void *data, int len) {
    return LoadTextureData([data, len](BaseTexData *tex_data) {
        return stbi_load_from_memory((const stbi_uc *) data, len, &tex_data->Width,
                                     &tex_data->Height, nullptr, tex_data->Channels);
    });
}

void AndroidImgui::DeleteTexture(BaseTexData *tex_data) {
    RemoveTexture(tex_data);
    auto it = std::find(m_Textures.begin(), m_Textures.end(), tex_data);
    if (it != m_Textures.end()) {
        m_Textures.erase(it);
    }
}
