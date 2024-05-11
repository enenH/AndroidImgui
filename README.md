# AndroidImgui

#### 介绍

[Dear ImGui](https://github.com/ocornut/imgui.git)以`opengles`和`vulkan`为后端在android平台上的封装，支持加载图片纹理等，两个后端可以无缝切换，可以开箱即用

#### 使用

```cmake
set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

FetchContent_Declare(
        AndroidImgui  # 你的库名
        GIT_REPOSITORY https://github.com/enenH/AndroidImgui.git  # GitHub库的URL
        GIT_TAG v1.6 # 你想要的分支或标签
)

FetchContent_MakeAvailable(AndroidImgui)  # 下载并使库可用


target_link_libraries(your_lib_name AndroidImgui)  # 链接库

```

#### 示例

```cpp
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
    auto graphics = GraphicsManager::getGraphicsInterface(GraphicsManager::VULKAN);

    graphics->Init(window, display.width, display.width);
    ImGui::Android_LoadSystemFont(26);
    Touch::Init({(float) display.width, (float) display.height}, true);

    auto image = graphics->LoadTextureFromFile("/sdcard/1.png");

    static bool flag = true;
    while (flag) {
        graphics->NewFrame();
        Touch::setOrientation(android::ANativeWindowCreator::GetDisplayInfo().orientation);
        ImGui::SetNextWindowSize({500, 500}, ImGuiCond_Once);
        if (ImGui::Begin("test", &flag)) {
            ImGui::Text("Hello, world!");
            ImGui::Image(image->DS, {100, 100});
        }
        ImGui::End();

        graphics->EndFrame();
    }
    graphics->DeleteTexture(image);
    graphics->Shutdown();
    return 0;
}


```

