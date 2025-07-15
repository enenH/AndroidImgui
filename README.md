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
        GIT_TAG v2.0 # 你想要的分支或标签
)

FetchContent_MakeAvailable(AndroidImgui)  # 下载并使库可用


target_link_libraries(your_lib_name AndroidImgui)  # 链接库

```

#### 示例

```cpp
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

#### 使用Java示例

```cpp
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

```

#### __可以被录制!!!!!!!!!!!__

```cpp
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
```