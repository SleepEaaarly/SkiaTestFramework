//
// Created by zeng on 2025/11/28.
//

#include "VulkanSurface.h"
#include "VulkanManager.h"
#include <cassert>
#include "include/gpu/ganesh/vk/GrVkBackendSemaphore.h"
#include "include/gpu/ganesh/vk/GrVkBackendSurface.h"
#include "include/gpu/ganesh/vk/GrVkTypes.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/MutableTextureState.h"
#include "include/gpu/vk/VulkanMutableTextureState.h"


VulkanSurface::~VulkanSurface() {
    if (fVkSwapChain != VK_NULL_HANDLE) {
        auto& vm = VulkanManager::getInstance();
        vm.fDeviceWaitIdle(vm.fDevice);
        vm.fDestroySwapchainKHR(vm.fDevice, fVkSwapChain, nullptr);
        fVkSwapChain = VK_NULL_HANDLE;
    }
    if (fVkSurface != VK_NULL_HANDLE) {
        VulkanManager::getInstance().unregisterSurface(fVkSurface);
        fVkSurface = VK_NULL_HANDLE;
    }

    // VulkanManager::getInstance().fDestroySwapchainKHR(VulkanManager::getInstance().fDevice, fVkSwapChain, nullptr);
}

VulkanSurface::VulkanSurface(ANativeWindow *window, GrDirectContext *grContext, ColorMode colorMode,
                             SkColorType colorType, sk_sp<SkColorSpace> colorSpace)
                             : fNativeWindow(window)
                             , fGrContext(grContext)
                             , fColorMode(colorMode)
                             , fColorType(colorType)
                             , fColorSpace(std::move(colorSpace)) {
    createVkSurface();
    createVkSwapChain();
    VulkanManager::getInstance().registerSurface(fVkSurface);
}

void VulkanSurface::createVkSurface() {
    VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo = {
            VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
            nullptr,
            0,
            fNativeWindow
    };

    VulkanManager::getInstance().fCreateAndroidSurfaceKHR(VulkanManager::getInstance().fInstance,&surfaceCreateInfo, nullptr, &fVkSurface);
}

bool VulkanSurface::createVkSwapChain() {
    VkResult err;

    VkSwapChainInfo swapChainInfo;

    // query swapchain support
    err = VulkanManager::getInstance().fGetPhysicalDeviceSurfaceCapabilitiesKHR(VulkanManager::getInstance().fPhysicalDevice, fVkSurface, &swapChainInfo.capabilities);
    if (err != VK_SUCCESS)
        return false;

    uint32_t formatCnt = 0;
    err = VulkanManager::getInstance().fGetPhysicalDeviceSurfaceFormatsKHR(VulkanManager::getInstance().fPhysicalDevice, fVkSurface, &formatCnt, nullptr);
    if (err != VK_SUCCESS)
        return false;
    swapChainInfo.formats.resize(formatCnt);
    VulkanManager::getInstance().fGetPhysicalDeviceSurfaceFormatsKHR(VulkanManager::getInstance().fPhysicalDevice, fVkSurface, &formatCnt, swapChainInfo.formats.data());

    uint32_t presentModeCnt = 0;
    VulkanManager::getInstance().fGetPhysicalDeviceSurfacePresentModesKHR(VulkanManager::getInstance().fPhysicalDevice, fVkSurface, &presentModeCnt, nullptr);
    swapChainInfo.presentModes.resize(presentModeCnt);
    VulkanManager::getInstance().fGetPhysicalDeviceSurfacePresentModesKHR(VulkanManager::getInstance().fPhysicalDevice, fVkSurface, &presentModeCnt, swapChainInfo.presentModes.data());

    // choose Surface Format
    VkSurfaceFormatKHR chosenFormat;
    for (const auto& avaliableFormat : swapChainInfo.formats) {
        if (avaliableFormat.format == VK_FORMAT_R8G8B8A8_UNORM && avaliableFormat.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
            chosenFormat = avaliableFormat;
            break;
        }
    }

    // choose Surface Present Mode
    VkPresentModeKHR chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR; //直接选用FIFO

    // choose swap extent
    VkExtent2D chosenExtent;
    if (swapChainInfo.capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        chosenExtent = swapChainInfo.capabilities.currentExtent;
    } else {
        assert(false);
    }
    fWidth = (int)chosenExtent.width;
    fHeight = (int)chosenExtent.height;

    // choose image count
    uint32_t chosenImageCnt = swapChainInfo.capabilities.minImageCount + 2;
    if (swapChainInfo.capabilities.maxImageCount > 0 && chosenImageCnt > swapChainInfo.capabilities.maxImageCount) {
        chosenImageCnt = swapChainInfo.capabilities.maxImageCount;
    }

    VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                   VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                    VK_IMAGE_USAGE_SAMPLED_BIT;
    assert((swapChainInfo.capabilities.supportedUsageFlags & usageFlags) == usageFlags); //必须要支持的Usage
//    if (swapChainInfo.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT) {
//        usageFlags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
//    }

    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    VkSwapchainCreateInfoKHR swapChainCreateInfo = {
            VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            nullptr,
            0,
            fVkSurface,
            chosenImageCnt,
            chosenFormat.format,
            chosenFormat.colorSpace,
            chosenExtent,
            1,
            usageFlags,
            VK_SHARING_MODE_EXCLUSIVE,
            0,
            nullptr,
            swapChainInfo.capabilities.currentTransform,
            compositeAlpha,
            chosenPresentMode,
            VK_TRUE,
            fVkSwapChain
    };

    uint32_t queueFamilies[] = {VulkanManager::getInstance().fGraphicQueueIndex, VulkanManager::getInstance().fPresentQueueIndex};
    if (VulkanManager::getInstance().fGraphicQueueIndex != VulkanManager::getInstance().fPresentQueueIndex) {
        swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapChainCreateInfo.queueFamilyIndexCount = 2;
        swapChainCreateInfo.pQueueFamilyIndices = queueFamilies;
    } else {
        swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapChainCreateInfo.queueFamilyIndexCount = 0;
        swapChainCreateInfo.pQueueFamilyIndices = nullptr;
    }

    err = VulkanManager::getInstance().fCreateSwapchainKHR(VulkanManager::getInstance().fDevice, &swapChainCreateInfo, nullptr, &fVkSwapChain);
    assert(err == VK_SUCCESS);

    if (swapChainCreateInfo.oldSwapchain != VK_NULL_HANDLE) {
        VulkanManager::getInstance().fDeviceWaitIdle(VulkanManager::getInstance().fDevice);

        // todo: destroy buffers
        destroyBuffers();
        VulkanManager::getInstance().fDestroySwapchainKHR(VulkanManager::getInstance().fDevice, swapChainCreateInfo.oldSwapchain,
                                         nullptr);
    }

    if (!createBuffers(swapChainCreateInfo.imageFormat, usageFlags, fColorType, swapChainCreateInfo.imageSharingMode)) {
        VulkanManager::getInstance().fDeviceWaitIdle(VulkanManager::getInstance().fDevice);

        destroyBuffers();
        VulkanManager::getInstance().fDestroySwapchainKHR(VulkanManager::getInstance().fDevice, fVkSwapChain,
                                         nullptr);
    }

    return true;
}

bool VulkanSurface::createBuffers(VkFormat format,
                                  VkImageUsageFlags usageFlags,
                                  SkColorType colorType,
                                  VkSharingMode sharingMode) {
    // acquire swapchain images
    VulkanManager::getInstance().fGetSwapchainImagesKHR(VulkanManager::getInstance().fDevice, fVkSwapChain, &fSwapChainImageCount,
                                           nullptr);
    assert(fSwapChainImageCount > 0);
    fSwapChainImages.resize(fSwapChainImageCount);
    VulkanManager::getInstance().fGetSwapchainImagesKHR(VulkanManager::getInstance().fDevice, fVkSwapChain, &fSwapChainImageCount, fSwapChainImages.data());

    // set up initial image layouts and create surfaces
    fSwapChainViews.resize(fSwapChainImageCount);
    fSwapChainImageLayouts.resize(fSwapChainImageCount);
    fSurfaces.resize(fSwapChainImageCount);
    for (uint32_t i = 0; i < fSwapChainImageCount; i++) {
        fSwapChainImageLayouts[i] = VK_IMAGE_LAYOUT_UNDEFINED;

        GrVkImageInfo imageInfo;
        imageInfo.fImage = fSwapChainImages[i];
        imageInfo.fAlloc = skgpu::VulkanAlloc();
        imageInfo.fImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.fImageTiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.fFormat = format;
        imageInfo.fImageUsageFlags = usageFlags;
        imageInfo.fLevelCount = 1;
        imageInfo.fCurrentQueueFamily = VulkanManager::getInstance().fPresentQueueIndex;
        imageInfo.fProtected = skgpu::Protected::kNo;
        imageInfo.fSharingMode = sharingMode;

        // if (usageFlags & VK_IMAGE_USAGE_SAMPLED_BIT) {
            GrBackendTexture backendTexture = GrBackendTextures::MakeVk(fWidth, fHeight, imageInfo);
            fSurfaces[i] = SkSurfaces::WrapBackendTexture(fGrContext, backendTexture, kTopLeft_GrSurfaceOrigin,1,fColorType,
                                                          fColorSpace, &fSurfaceProps);
        // }
    }

    VkSemaphoreCreateInfo semaphoreCreateInfo;
    memset(&semaphoreCreateInfo, 0, sizeof(VkSemaphoreCreateInfo));
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = nullptr;
    semaphoreCreateInfo.flags = 0; // 目前版本Vulkan，这个字段还没有任何作用

    // 创建用于Present的后缓冲结构
    fBackBuffers.resize(fSwapChainImageCount + 1);
    for (uint32_t i = 0; i < fSwapChainImageCount + 1; i++) {
        fBackBuffers[i].fImageIndex = -1; //初始化为无效
        VkResult err = VulkanManager::getInstance().fCreateSemaphore(VulkanManager::getInstance().fDevice, &semaphoreCreateInfo, nullptr, &fBackBuffers[i].fRenderSemaphore);
        assert(err == VK_SUCCESS);
    }
    fCurrentBackBufferindex = fSwapChainImageCount;

    return true;
}

void VulkanSurface::destroyBuffers() {
    if (fBackBuffers.size() > 0) {
        for (uint32_t i = 0; i < fSwapChainImageCount + 1; ++i) {
            fBackBuffers[i].fImageIndex = -1;
            VulkanManager::getInstance().fDestroySemaphore(VulkanManager::getInstance().fDevice, fBackBuffers[i].fRenderSemaphore,
                                              nullptr);
        }
    }
    fBackBuffers.clear();
    fSurfaces.clear();
    fSwapChainImageLayouts.clear();
    fSwapChainImages.clear();
}

VulkanSurface* VulkanSurface::Create(ANativeWindow* window, ColorMode colorMode, SkColorType colorType,
                                     sk_sp<SkColorSpace> colorSpace, GrDirectContext* grContext,
                                     uint32_t extraBuffers) {
    return new VulkanSurface(window, grContext, colorMode, colorType, colorSpace);
}

VulkanSurface::BackbufferInfo* VulkanSurface::getAvailableBackbuffer() {
    assert(fBackBuffers.size() > 0);
    ++fCurrentBackBufferindex;
    if (fCurrentBackBufferindex > fSwapChainImageCount) {
        fCurrentBackBufferindex = 0;
    }
    BackbufferInfo* backbufferInfo = &fBackBuffers[fCurrentBackBufferindex];
    return backbufferInfo;
}

//#include <android/log.h>
//#define LOG_TAG "Debug"                 // 在 logcat 里用来过滤
//#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
//#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
//#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)

sk_sp<SkSurface> VulkanSurface::getBackbufferSurface() {
    BackbufferInfo* backbufferInfo = getAvailableBackbuffer();
    assert(backbufferInfo);

    VkSemaphoreCreateInfo semaphoreCreateInfo;
    memset(&semaphoreCreateInfo, 0, sizeof(VkSemaphoreCreateInfo));
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = nullptr;
    semaphoreCreateInfo.flags = 0;

    // here, we create semaphore because we can only draw surface in skia after acquire image from swapchain
    // in other words, we are controlling the "acquire-write" order of image
    VkSemaphore semaphore;
    VkResult err = VulkanManager::getInstance().fCreateSemaphore(VulkanManager::getInstance().fDevice, &semaphoreCreateInfo, nullptr, &semaphore);
    assert(err == VK_SUCCESS);

    err = VulkanManager::getInstance().fAcquireNextImageKHR(VulkanManager::getInstance().fDevice, fVkSwapChain, UINT64_MAX, semaphore, VK_NULL_HANDLE, &backbufferInfo->fImageIndex);
    if (err == VK_ERROR_SURFACE_LOST_KHR) {
        VulkanManager::getInstance().fDestroySemaphore(VulkanManager::getInstance().fDevice, semaphore, nullptr);
        return nullptr;
    }
    if (VK_ERROR_OUT_OF_DATE_KHR == err) {
        if (!createVkSwapChain()) {
            VulkanManager::getInstance().fDestroySemaphore(VulkanManager::getInstance().fDevice, semaphore, nullptr);
            return nullptr;
        }

        backbufferInfo = getAvailableBackbuffer();
        err = VulkanManager::getInstance().fAcquireNextImageKHR(VulkanManager::getInstance().fDevice, fVkSwapChain, UINT64_MAX, semaphore, VK_NULL_HANDLE, &backbufferInfo->fImageIndex);
        if (err != VK_SUCCESS) {
            VulkanManager::getInstance().fDestroySemaphore(VulkanManager::getInstance().fDevice, semaphore, nullptr);
            return nullptr;
        }
    }

    SkSurface* surface = fSurfaces[backbufferInfo->fImageIndex].get();
    GrBackendSemaphore backendSemaphore = GrBackendSemaphores::MakeVk(semaphore);

    // LOGI("Surface!!!");
    surface->wait(1,&backendSemaphore);

    return sk_ref_sp(surface);
}

void VulkanSurface::swapBuffers() {
    BackbufferInfo backbufferInfo = fBackBuffers[fCurrentBackBufferindex];
    SkSurface* surface = fSurfaces[backbufferInfo.fImageIndex].get();

    GrBackendSemaphore backendSemaphore = GrBackendSemaphores::MakeVk(backbufferInfo.fRenderSemaphore);

    GrFlushInfo flushInfo;
    flushInfo.fNumSemaphores = 1;
    flushInfo.fSignalSemaphores = &backendSemaphore;
    skgpu::MutableTextureState presentState = skgpu::MutableTextureStates::MakeVulkan(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VulkanManager::getInstance().fPresentQueueIndex);

    auto dContext = surface->recordingContext()->asDirectContext();
    dContext->flush(surface, flushInfo, &presentState);
    dContext->submit();

    const VkPresentInfoKHR presentInfo = {
            VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            nullptr,
            1,
            &backbufferInfo.fRenderSemaphore, //等待渲染的Semaphore
            1,
            &fVkSwapChain,
            &backbufferInfo.fImageIndex,
            nullptr
    };
    VulkanManager::getInstance().fQueuePresentKHR(VulkanManager::getInstance().fPresentQueue, &presentInfo);
}
