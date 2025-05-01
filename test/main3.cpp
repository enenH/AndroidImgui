#include <thread>
#include <unistd.h>
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
    jobject view = JavaFunc::getView(display.width, display.width, false, false);
    std::thread([&] {
        jobject surface = nullptr;
        for (int i = 0; i < 5; i++) {
            sleep(1);
            surface = JavaFunc::getSurface(view);
            if (surface) {
                break;
            }
        }
        auto window = ANativeWindow_fromSurface(JavaFunc::GetJavaEnv(), surface);
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
                ImGui::Text("Hello, world!!");
            }
            ImGui::End();

            graphics->EndFrame();
        }
        graphics->Shutdown();
        JavaFunc::removeView(view);
        sleep(1);
        exit(0);
    }).detach();
    JavaFunc::loop();

    return 0;
}
