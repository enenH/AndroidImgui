//
// Created by ITEK on 2024/2/3.
//

#include "GraphicsManager.h"
#include "VulkanGraphics.h"
#include "OpenGLGraphics.h"


std::unique_ptr<AndroidImgui> GraphicsManager::getGraphicsInterface(GraphicsAPI api) {
    switch (api) {
        case OPENGL:
            return std::make_unique<OpenGLGraphics>();
        case VULKAN:
            return std::make_unique<VulkanGraphics>();
    }
    return nullptr;
}
