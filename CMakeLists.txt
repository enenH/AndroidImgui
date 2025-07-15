cmake_minimum_required(VERSION 3.22)

if (NOT DEFINED CMAKE_ANDROID_NDK)
    set(CMAKE_ANDROID_NDK D:/Android/SDK/ndk/27.0.11718014)
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_ANDROID_NDK}/build/cmake/android.toolchain.cmake)
    set(CMAKE_SYSTEM_NAME Android)
    set(CMAKE_SYSTEM_VERSION 24)
    set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(ANDROID_ABI arm64-v8a)
    set(ANDROID_PLATFORM android-26)
    set(ANDROID_NDK ${CMAKE_ANDROID_NDK})
endif ()

project(AndroidImgui)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(freetype)

include_directories(freetype/include)

file(GLOB SOURCES src/*.cpp)

add_library(AndroidImgui STATIC
        ${SOURCES}

        stb/stb_image.c
        src/ELF/elf_util.cpp
        src/Jenv/JavaFunc.cpp

        imgui/imgui.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_widgets.cpp
        imgui/imgui_tables.cpp
        imgui/imgui_demo.cpp
        imgui/backends/imgui_impl_opengl3.cpp
        imgui/backends/imgui_impl_vulkan.cpp
        imgui/misc/freetype/imgui_freetype.cpp

)

target_include_directories(AndroidImgui PUBLIC
        src
        imgui
        imgui/backends
        stb
)

target_compile_definitions(AndroidImgui PUBLIC
        VK_USE_PLATFORM_ANDROID_KHR
        IMGUI_IMPL_VULKAN_NO_PROTOTYPES
        IMGUI_ENABLE_FREETYPE)

target_link_libraries(AndroidImgui
        log
        android
        EGL
        GLESv3
        z
        freetype)


#[[add_executable(AndroidImguiTest
        test/main3.cpp
        test/TouchHelperA.cpp)

target_link_libraries(AndroidImguiTest
        AndroidImgui
)]]


#[[git submodule update --remote imgui]]
