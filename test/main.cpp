#include "AndroidImgui.h"
#include "ANativeWindowCreator.h"
#include "GraphicsManager.h"
#include "imgui.h"
#include "my_imgui.h"
#include "TouchHelperA.h"

int main() {
    auto display = android::ANativeWindowCreator::GetDisplayInfo();
    if (display.height > display.width) {
        std::swap(display.height, display.width);
    }
    auto window = android::ANativeWindowCreator::Create("test", display.width, display.width);
    auto graphics = GraphicsManager::getGraphicsInterface(GraphicsManager::VULKAN);

    graphics->Init(window, display.width, display.width);
    ImGui::Android_LoadSystemFont(26);
    Touch::Init({(float) display.width, (float) display.height}, true);

    auto image = graphics->LoadTextureFromFile("/sdcard/1.png");

    static bool flag = true;
    while (flag) {
        graphics->NewFrame();

        ImGui::SetNextWindowSize({500, 500}, ImGuiCond_Once);
        if (ImGui::Begin("test", &flag)) {
            ImGui::Text("Hello, world!");
            ImGui::Image(image->DS, {100, 100});
        }
        ImGui::End();

        graphics->EndFrame();
    }
    graphics->Shutdown();
    return 0;
}
