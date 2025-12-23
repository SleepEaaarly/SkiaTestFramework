//
// Created by zeng on 2025/11/26.
//

#ifndef SKIAVULKAN_VULKANMANAGER_H
#define SKIAVULKAN_VULKANMANAGER_H

#include "../Singleton.h"
#include "include/gpu/ganesh/GrDirectContext.h"
// #include "include/gpu/vk/VulkanBackendContext.h"
#include "GrVkExtensions.h"

#include <memory>
#include <vulkan/vulkan.h>
#include "include/core/SkColorSpace.h"
#include "../ColorMode.h"

#include <unordered_set>

// #include "VulkanSurface.h"

class VulkanSurface;

class VulkanManager : public Singleton<VulkanManager> {
    friend class Singleton<VulkanManager>;      // Important!!! for Singleton<VulkanManager> to ask for VulkanManager private functions
    friend class VulkanSurface;
public:

    bool hasVkContext() const { return fDevice != VK_NULL_HANDLE; }

    void initialize();

    uint32_t getDriverVersion() const { return fDriverVersion; }

    sk_sp<GrDirectContext> createContext(GrContextOptions& options);

    VulkanSurface* createSurface(ANativeWindow* window,
                                 ColorMode colorMode,
                                 sk_sp<SkColorSpace> surfaceColorSpace,
                                 SkColorType surfaceColorType,
                                 GrDirectContext* grContext,
                                 uint32_t extraBuffers);

    void destroySurface(VulkanSurface* surface);

private:

    ~VulkanManager();

    void setupDevice(GrVkExtensions& extensions, VkPhysicalDeviceFeatures2& feature);

    void registerSurface(VkSurfaceKHR surface);
    void unregisterSurface(VkSurfaceKHR surface);
    bool hasAliveSurface();

private:

    template<typename FNPTR_TYPE>
    class VkPtr {
    public:
        VkPtr() : fPtr(nullptr) {}
        VkPtr operator=(FNPTR_TYPE ptr) {
            fPtr = ptr;
            return *this;
        }

        operator FNPTR_TYPE() const { return fPtr; }

    private:
        FNPTR_TYPE fPtr;
    };

    std::unordered_set<VkSurfaceKHR> fAliveSurfaces;
    std::mutex fSurfaceLock;

    static const uint32_t fAPIVersion = VK_MAKE_VERSION(1,1,0);
    uint32_t fDriverVersion = 0;
    VkInstance fInstance = VK_NULL_HANDLE;
    VkPhysicalDevice fPhysicalDevice = VK_NULL_HANDLE;
    VkDevice fDevice = VK_NULL_HANDLE;
    std::mutex fInitializeLock;
    uint32_t fGraphicQueueIndex = UINT32_MAX;
    uint32_t fPresentQueueIndex = UINT32_MAX;
    VkQueue fGraphicQueue;
    VkQueue fAHBUploadQueue;
    VkQueue fPresentQueue;

    GrVkExtensions fExtensions;
    VkPhysicalDeviceFeatures2 fPhysicalDeviceFeatures2{};

    std::vector<VkExtensionProperties> fInstanceExtensionsOwner;
    std::vector<const char*> fInstanceExtensions;
    std::vector<VkExtensionProperties> fDeviceExtensionOwner;
    std::vector<const char*> fDeviceExtensions;
    int fContextPriority = 0;

    VkPtr<PFN_vkEnumerateInstanceVersion> fEnumerateInstanceVersion;
    VkPtr<PFN_vkEnumerateInstanceExtensionProperties> fEnumerateInstanceExtensionProperties;
    VkPtr<PFN_vkCreateInstance> fCreateInstance;
    VkPtr<PFN_vkCreateDevice> fCreateDevice;
    VkPtr<PFN_vkDestroyDevice> fDestroyDevice;
    VkPtr<PFN_vkEnumerateDeviceExtensionProperties> fEnumerateDeviceExtensionProperties;
    VkPtr<PFN_vkEnumeratePhysicalDevices> fEnumeratePhysicalDevices;
    VkPtr<PFN_vkGetPhysicalDeviceFeatures2> fGetPhysicalDeviceFeatures2;
    VkPtr<PFN_vkGetPhysicalDeviceImageFormatProperties2> fGetPhysicalDeviceImageFormatProperties2;
    VkPtr<PFN_vkGetPhysicalDeviceProperties> fGetPhysicalDeviceProperties;
    VkPtr<PFN_vkGetPhysicalDeviceQueueFamilyProperties> fGetPhysicalDeviceQueueFamilyProperties;

    VkPtr<PFN_vkAllocateCommandBuffers> fAllocateCommandBuffers;
    VkPtr<PFN_vkBeginCommandBuffer> fBeginCommandBuffer;
    VkPtr<PFN_vkCmdPipelineBarrier> fCmdPipelineBarrier;
    VkPtr<PFN_vkCreateCommandPool> fCreateCommandPool;
    VkPtr<PFN_vkCreateFence> fCreateFence;
    VkPtr<PFN_vkCreateSemaphore> fCreateSemaphore;
    VkPtr<PFN_vkDestroyCommandPool> fDestroyCommandPool;
    VkPtr<PFN_vkDestroyFence> fDestroyFence;
    VkPtr<PFN_vkDestroySemaphore> fDestroySemaphore;
    VkPtr<PFN_vkDeviceWaitIdle> fDeviceWaitIdle;
    VkPtr<PFN_vkEndCommandBuffer> fEndCommandBuffer;
    VkPtr<PFN_vkFreeCommandBuffers> fFreeCommandBuffers;
    VkPtr<PFN_vkGetDeviceQueue> fGetDeviceQueue;
    VkPtr<PFN_vkGetSemaphoreFdKHR> fGetSemaphoreFdKHR;
    VkPtr<PFN_vkImportFenceFdKHR> fImportFenceFdKHR;
    VkPtr<PFN_vkImportSemaphoreFdKHR> fImportSemaphoreFdKHR;
    VkPtr<PFN_vkQueueSubmit> fQueueSubmit;
    VkPtr<PFN_vkQueueWaitIdle> fQueueWaitIdle;
    VkPtr<PFN_vkResetCommandBuffer> fResetCommandBuffer;
    VkPtr<PFN_vkResetFences> fResetFences;
    VkPtr<PFN_vkWaitForFences> fWaitForFences;
    VkPtr<PFN_vkCreateAndroidSurfaceKHR> fCreateAndroidSurfaceKHR;
    VkPtr<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR> fGetPhysicalDeviceSurfaceCapabilitiesKHR;
    VkPtr<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR> fGetPhysicalDeviceSurfaceFormatsKHR;
    VkPtr<PFN_vkCreateSwapchainKHR> fCreateSwapchainKHR;
    VkPtr<PFN_vkGetSwapchainImagesKHR> fGetSwapchainImagesKHR;
    VkPtr<PFN_vkDestroySwapchainKHR> fDestroySwapchainKHR;
    VkPtr<PFN_vkQueuePresentKHR> fQueuePresentKHR;
    VkPtr<PFN_vkAcquireNextImageKHR> fAcquireNextImageKHR;
    VkPtr<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR> fGetPhysicalDeviceSurfacePresentModesKHR;
    VkPtr<PFN_vkGetBufferMemoryRequirements2> fGetBufferMemoryRequirements2;

    std::atomic_bool fInitialized = false;

};


#endif //SKIAVULKAN_VULKANMANAGER_H
