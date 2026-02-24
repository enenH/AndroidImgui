//
// Created by ITEK on 2024/2/3.
//

#include "GraphicsManager.h"
#include "VulkanGraphics.h"
#include "OpenGLGraphics.h"
#include "SoftwareGraphics.h"


std::unique_ptr<AndroidImgui> GraphicsManager::getGraphicsInterface(GraphicsAPI api) {
    switch (api) {
        case OPENGL:
            return std::make_unique<OpenGLGraphics>();
        case VULKAN:
            return std::make_unique<VulkanGraphics>();
        case SOFTWARE:
            return std::make_unique<SoftwareGraphics>();
    }
    return nullptr;
}
