#include <android/native_window_jni.h>
#include "Jenv/JavaFunc.h"
#include "GraphicsManager.h"
#include "my_imgui.h"
#include "TouchHelperA.h"

int main() {
    if (JavaFunc::init() < 0) {
        printf("JavaFunc init failed\n");
        return -1;
    }
    auto display = JavaFunc::getDisplayInfo();
    if (display.height > display.width) {
        std::swap(display.height, display.width);
    }
    auto jwindow = JavaFunc::createNativeWindow(display.width, display.width, true, false);
    auto window = ANativeWindow_fromSurface(JavaFunc::GetJavaEnv(), jwindow);
    auto graphics = GraphicsManager::getGraphicsInterface(GraphicsManager::VULKAN);
    graphics->Init(window, display.width, display.width);
    ImGui::Android_LoadSystemFont(26);
    Touch::Init({(float)display.width, (float)display.height}, true);

    static bool flag = true;
    while (flag) {
        graphics->NewFrame();
        Touch::setOrientation(JavaFunc::getDisplayInfo().orientation);
        ImGui::SetNextWindowSize({500, 500}, ImGuiCond_Once);
        if (ImGui::Begin("test", &flag)) {
            ImGui::Text("Hello, world!");
            ImGui::Text("Cliboard: %s", JavaFunc::getClipboardText().c_str());
        }
        ImGui::End();

        graphics->EndFrame();
    }
    graphics->Shutdown();
    JavaFunc::destroyNativeWindow(jwindow);
    return 0;
}
