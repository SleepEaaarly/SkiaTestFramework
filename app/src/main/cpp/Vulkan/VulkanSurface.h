//
// Created by zeng on 2025/11/28.
//

#ifndef SKIAVULKAN_VULKANSURFACE_H
#define SKIAVULKAN_VULKANSURFACE_H

#include <vulkan/vulkan.h>
#include "include/core/SkColorSpace.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/core/SkSurfaceProps.h"
#include "../ColorMode.h"
#include "include/core/SkSurface.h"

class VulkanManager;
struct ANativeWindow;

class VulkanSurface {
public:

    ~VulkanSurface();

    static VulkanSurface* Create(ANativeWindow* window, ColorMode colorMode, SkColorType colorType,
                                 sk_sp<SkColorSpace> colorSpace, GrDirectContext* grContext,
                                 uint32_t extraBuffers);

    sk_sp<SkSurface> getBackbufferSurface();

    void swapBuffers();

private:
    struct VkSwapChainInfo {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct BackbufferInfo {
        uint32_t        fImageIndex;
        VkSemaphore     fRenderSemaphore;
    };
    BackbufferInfo* getAvailableBackbuffer();

    void createVkSurface();
    bool createVkSwapChain();
    bool createBuffers(VkFormat format,VkImageUsageFlags usageFlags,
                       SkColorType colorType,
                       VkSharingMode sharingMode);
    void destroyBuffers();
//    friend class VulkanManager;
    VulkanSurface(ANativeWindow* window, GrDirectContext* grContext,  ColorMode colorMode, SkColorType colorType,
                  sk_sp<SkColorSpace> colorSpace);

private:
    ANativeWindow* fNativeWindow;
    GrDirectContext* fGrContext;
    VkSurfaceKHR fVkSurface;
    VkExtent2D fDisplaySize;
    VkFormat fDisplayFormat;
    VkSwapchainKHR fVkSwapChain = VK_NULL_HANDLE;
    uint32_t fSwapChainImageCount = 0;
    std::vector<VkImage> fSwapChainImages;
    std::vector<VkImageLayout> fSwapChainImageLayouts;
    std::vector<VkImageView> fSwapChainViews;
    std::vector<sk_sp<SkSurface>> fSurfaces;
    std::vector<BackbufferInfo> fBackBuffers;
    uint32_t fCurrentBackBufferindex;
    uint32_t fWidth = 0;
    uint32_t fHeight = 0;

    ColorMode fColorMode;
    SkColorType fColorType;
    sk_sp<SkColorSpace> fColorSpace;
    SkSurfaceProps fSurfaceProps{0, kRGB_H_SkPixelGeometry};


    // array of frame buffers and views
    std::vector<VkImage> fDisplayImages;
    std::vector<VkImageView> fDdisplayViews;
    std::vector<VkFramebuffer> fFramebuffers;
};


#endif //SKIAVULKAN_VULKANSURFACE_H
