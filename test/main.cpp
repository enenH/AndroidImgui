#include <unistd.h>

#include "ANativeWindowCreator.h"
#include "GraphicsManager.h"
#include "my_imgui.h"
#include "TouchHelperA.h"

int main() {
    auto display = android::ANativeWindowCreator::GetDisplayInfo();
    if (display.height > display.width) {
        std::swap(display.height, display.width);
    }
    auto window = android::ANativeWindowCreator::Create("test", display.width, display.width);
    auto graphics = GraphicsManager::getGraphicsInterface(GraphicsManager::SOFTWARE);

    graphics->Init(window, display.width, display.width);
    ImGui::Android_LoadSystemFont(26);
    Touch::Init({(float)display.width, (float)display.height}, true);

    static bool flag = true;
    while (flag) {
        graphics->NewFrame();
        Touch::setOrientation(android::ANativeWindowCreator::GetDisplayInfo().orientation);
        ImGui::SetNextWindowSize({500, 500}, ImGuiCond_Once);
        if (ImGui::Begin("test", &flag)) {
            ImGui::Text("%.1f ms %.1f FPS", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

            ImGui::Text("漨悋孄憀弖嫮廼怮圜爘楱濲墈垉");
        }
        ImGui::End();
        graphics->EndFrame();
    }
    graphics->Shutdown();
    android::ANativeWindowCreator::Destroy(window);
    sleep(1);
    return 0;
}
