//
// Created by zeng on 2025/11/26.
//

#ifndef SKIAVULKAN_SKIAVULKANPIPELINE_H
#define SKIAVULKAN_SKIAVULKANPIPELINE_H

#include "../SkiaPipeline.h"
#include "VulkanManager.h"
#include <vulkan/vulkan.h>
#include "VulkanSurface.h"

class SkiaVulkanPipeline : public SkiaPipeline {
public:
    explicit SkiaVulkanPipeline();

    bool setSurface(ANativeWindow* window) override;

    void draw() override;

protected:
    void renderFrame(SkCanvas* canvas) override;

private:
    void requireVkContext();

    VulkanManager& vulkanManager();

private:
    VulkanSurface* fVkSurface = nullptr;

    ColorMode fColorMode = ColorMode::Default;
    SkColorType fSurfaceColorType;
    sk_sp<SkColorSpace> fSurfaceColorSpace;
    ANativeWindow* fNativeWindow;
};


#endif //SKIAVULKAN_SKIAVULKANPIPELINE_H
