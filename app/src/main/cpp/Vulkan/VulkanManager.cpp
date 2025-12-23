//
// Created by zeng on 2025/11/26.
//

#include <assert.h>
#include "VulkanManager.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <android/log.h>
#include <sstream>
#include "include/gpu/ganesh/vk/GrVkDirectContext.h"
#include "include/gpu/vk/VulkanBackendContext.h"
#include "VulkanSurface.h"

extern std::ostream aout;
using GrVkBackendContext = skgpu::VulkanBackendContext;

#define GET_PROC(F) f##F = (PFN_vk##F)vkGetInstanceProcAddr(VK_NULL_HANDLE, "vk" #F)
#define GET_INSTANCE_PROC(F) f##F = (PFN_vk##F)vkGetInstanceProcAddr(fInstance, "vk" #F)
#define GET_DEVICE_PROC(F) f##F = (PFN_vk##F)vkGetDeviceProcAddr(fDevice, "vk" #F)
static std::array<std::string_view, 21> sEnableExtensions{
        VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
        VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
        VK_KHR_MAINTENANCE1_EXTENSION_NAME,
        VK_KHR_MAINTENANCE2_EXTENSION_NAME,
        VK_KHR_MAINTENANCE3_EXTENSION_NAME,
        VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_BLEND_OPERATION_ADVANCED_EXTENSION_NAME,
        VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
        VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
        VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME,
        VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
        VK_EXT_GLOBAL_PRIORITY_EXTENSION_NAME,
        VK_EXT_DEVICE_FAULT_EXTENSION_NAME,
};



void VulkanManager::initialize() {
    std::lock_guard _lock(fInitializeLock);

    if (fDevice != VK_NULL_HANDLE) {
        return ;
    }

    GET_PROC(EnumerateInstanceVersion);
    uint32_t instanceVersion;
    fEnumerateInstanceVersion(&instanceVersion);
    uint acVersion = VK_MAKE_VERSION(1, 1, 0);
    assert(instanceVersion >= acVersion && "Vulkan version is too low");

    setupDevice(fExtensions, fPhysicalDeviceFeatures2);

    fInitialized = true;
}

VulkanManager::~VulkanManager() {
    /* 0. 检测是否还有未销毁的 Surface */
    std::lock_guard<std::mutex> lock(fInitializeLock);
    if (!fInitialized) return;

    if (hasAliveSurface()) {
        __android_log_print(ANDROID_LOG_ERROR, "VulkanManager",
                            "FATAL: %zu VkSurfaceKHR still alive when destroying VkInstance!",
                            fAliveSurfaces.size());
        /* 开发期直接崩溃，方便立即定位 */
        assert(false && "VkSurfaceKHR leak detected, see logcat");
        abort();
    }

    /* 1. 等待所有队列空闲 */
    if (fDevice != VK_NULL_HANDLE) {
        if (fGraphicQueue) fQueueWaitIdle(fGraphicQueue);
        if (fPresentQueue && fPresentQueue != fGraphicQueue) fQueueWaitIdle(fPresentQueue);
    }

    /* 2. 销毁 Device（会连带销毁 queue、commandPool、fence、semaphore 等） */
    if (fDevice != VK_NULL_HANDLE) {
        fDestroyDevice(fDevice, nullptr);
        fDevice = VK_NULL_HANDLE;
    }

    /* 3. 释放扩展结构体 pNext 链 */
    VkBaseOutStructure* pNext = reinterpret_cast<VkBaseOutStructure*>(fPhysicalDeviceFeatures2.pNext);
    while (pNext) {
        VkBaseOutStructure* next = pNext->pNext;
        std::free(pNext);          // 对应 malloc
        pNext = next;
    }
    fPhysicalDeviceFeatures2.pNext = nullptr;

    /* 4. 销毁 Instance */
    if (fInstance != VK_NULL_HANDLE) {
        // 必须先于 instance 销毁 surface，但 surface 归 VulkanSurface 管，
        // 而 VulkanSurface 生命周期<=窗口生命周期，此时应已全部析构。
        // 若仍有未销毁的 surface，vkDestroyInstance 会报 validation warning，
        // 因此保证“先销毁所有 VulkanSurface”是外部职责。
        vkDestroyInstance(fInstance, nullptr);
        fInstance = VK_NULL_HANDLE;
    }

    fInitialized = false;
}

void VulkanManager::destroySurface(VulkanSurface* surface) {
    if (!fInitialized) {
        this->initialize();
    }
    if (VK_NULL_HANDLE != fGraphicQueue) {
        fQueueWaitIdle(fGraphicQueue);
    }
    delete surface;
}

static bool shouldEnableExtension(const std::string_view& extension) {
    for (const auto& it : sEnableExtensions) {
        if (it == extension) return true;
    }
    return false;
}

void VulkanManager::setupDevice(GrVkExtensions &grExtensions, VkPhysicalDeviceFeatures2 &feature) {
    VkResult err;

    constexpr VkApplicationInfo appInfo = {
            VK_STRUCTURE_TYPE_APPLICATION_INFO,
            nullptr,
            "Vulkan Skia Framework",
            0,
            "Vulkan Engine 1.1",
            0,
            fAPIVersion
    };

    // acquire Instance extension api
    GET_PROC(EnumerateInstanceExtensionProperties);

    uint32_t extensionCount = 0;
    err = fEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    assert(err == VK_SUCCESS && "EnumerateInstanceExtensionProperties failed");
    fInstanceExtensionsOwner.resize(extensionCount);
    fEnumerateInstanceExtensionProperties(nullptr, &extensionCount, fInstanceExtensionsOwner.data());
    assert(err == VK_SUCCESS && "EnumerateInstanceExtensionProperties failed");

    bool hasKHRSurfaceExtension = false;
    bool hasKHRAndroidSurfaceExtension = false;
    for (const VkExtensionProperties& extension : fInstanceExtensionsOwner) {
        if (!shouldEnableExtension(extension.extensionName)) {
            continue;
        }

        fInstanceExtensions.push_back(extension.extensionName);
        if (!strcmp(extension.extensionName, VK_KHR_SURFACE_EXTENSION_NAME)) {
            hasKHRSurfaceExtension = true;
        }

        if (!strcmp(extension.extensionName, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME)) {
            hasKHRAndroidSurfaceExtension = true;
        }
    }
    assert((hasKHRSurfaceExtension || hasKHRAndroidSurfaceExtension) && "Must have surface extension for on screen drawing");


    const VkInstanceCreateInfo instanceInfo = {
            VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            nullptr,
            0,
            &appInfo,
            0,
            nullptr,
            (uint32_t)fInstanceExtensions.size(),
            fInstanceExtensions.data()
    };

    GET_PROC(CreateInstance);
    err = fCreateInstance(&instanceInfo, nullptr, &fInstance);
    assert(err == VK_SUCCESS);

    GET_INSTANCE_PROC(CreateDevice);
    GET_INSTANCE_PROC(DestroyDevice);
    GET_INSTANCE_PROC(EnumerateDeviceExtensionProperties);
    GET_INSTANCE_PROC(EnumeratePhysicalDevices);
    GET_INSTANCE_PROC(GetPhysicalDeviceFeatures2);
    GET_INSTANCE_PROC(GetPhysicalDeviceImageFormatProperties2);
    GET_INSTANCE_PROC(GetPhysicalDeviceProperties);
    GET_INSTANCE_PROC(GetPhysicalDeviceQueueFamilyProperties);
    GET_INSTANCE_PROC(CreateAndroidSurfaceKHR);
    GET_INSTANCE_PROC(GetPhysicalDeviceSurfaceCapabilitiesKHR);
    GET_INSTANCE_PROC(GetPhysicalDeviceSurfaceFormatsKHR);
    GET_INSTANCE_PROC(GetPhysicalDeviceSurfacePresentModesKHR);

    uint32_t gpuCnt;
    err = fEnumeratePhysicalDevices(fInstance, &gpuCnt, nullptr);
    assert(err == VK_SUCCESS && gpuCnt != 0);

    // only use first gpu
    gpuCnt = 1;
    err = fEnumeratePhysicalDevices(fInstance, &gpuCnt, &fPhysicalDevice);
    assert(err == VK_SUCCESS && "vkEnumeratePhysicalDevices failed!");

    VkPhysicalDeviceProperties physDeviceProperties;
    fGetPhysicalDeviceProperties(fPhysicalDevice, &physDeviceProperties);
    assert(physDeviceProperties.apiVersion >= VK_MAKE_VERSION(1,1,0));
    fDriverVersion = physDeviceProperties.driverVersion;

    uint32_t queueCnt = 0;
    fGetPhysicalDeviceQueueFamilyProperties(fPhysicalDevice, &queueCnt, nullptr);
    assert(queueCnt > 0);

    std::unique_ptr<VkQueueFamilyProperties[]> queueFamilyProps(new VkQueueFamilyProperties[queueCnt]);
    fGetPhysicalDeviceQueueFamilyProperties(fPhysicalDevice, &queueCnt, queueFamilyProps.get());

    // find graphics and present queue
    auto canPresent = [](VkInstance, VkPhysicalDevice, uint32_t) { return true; };
    for (uint32_t i = 0; i < queueCnt; ++i) {
        if (fGraphicQueueIndex != UINT32_MAX && fPresentQueueIndex != UINT32_MAX) {
            break;
        }
        if (canPresent(fInstance, fPhysicalDevice, i)) {
            fPresentQueueIndex = i;
        }
        if (queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            fGraphicQueueIndex = i;
        }
    }
    assert(fGraphicQueueIndex < queueCnt);
    assert(fPresentQueueIndex < queueCnt);

    {
        uint32_t extensionCnt = 0;
        //不指定layer?
        err = fEnumerateDeviceExtensionProperties(fPhysicalDevice, nullptr, &extensionCnt,
                                                  nullptr);
        assert(err == VK_SUCCESS);

        fDeviceExtensionOwner.resize(extensionCnt);
        err = fEnumerateDeviceExtensionProperties(fPhysicalDevice, nullptr, &extensionCnt,
                                                  fDeviceExtensionOwner.data());
        assert(err == VK_SUCCESS);

        bool hasKHRSwapchainExtension = false;
        for (const VkExtensionProperties &extension: fDeviceExtensionOwner) {
            //跳过不需要的扩展
            if (!shouldEnableExtension(extension.extensionName)) {
                continue;
            }

            fDeviceExtensions.push_back(extension.extensionName);
            if (!strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
                hasKHRSwapchainExtension = true;
            }
        }
        assert(hasKHRSwapchainExtension);
    }

    auto getProc = [](const char* proc_name, VkInstance instance, VkDevice device) {
        //优先设备级扩展
        if (device != VK_NULL_HANDLE) {
            return vkGetDeviceProcAddr(device, proc_name);
        }
        return vkGetInstanceProcAddr(instance, proc_name);
    };
    grExtensions.init(getProc, fInstance, fPhysicalDevice, fInstanceExtensions.size(), fInstanceExtensions.data(),
                      fDeviceExtensions.size(), fDeviceExtensions.data());

    assert(grExtensions.hasExtension(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,1));

    memset(&feature, 0, sizeof(VkPhysicalDeviceFeatures2));
    feature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    feature.pNext = nullptr;

    void** tailPNext = &feature.pNext;
    if (grExtensions.hasExtension(VK_EXT_BLEND_OPERATION_ADVANCED_EXTENSION_NAME, 2)) {
        VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT* blend;
        blend = (VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT*)malloc(sizeof(VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT));
        blend->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_FEATURES_EXT;
        blend->pNext = nullptr;

        *tailPNext = blend;
        tailPNext = &blend->pNext;
    }

    VkPhysicalDeviceSamplerYcbcrConversionFeatures* ycbcFeature;
    ycbcFeature = (VkPhysicalDeviceSamplerYcbcrConversionFeatures*)malloc(
            sizeof(VkPhysicalDeviceSamplerYcbcrConversionFeatures));
    ycbcFeature->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES;
    ycbcFeature->pNext = nullptr;
    *tailPNext = ycbcFeature;
    tailPNext = &ycbcFeature->pNext;

    if (grExtensions.hasExtension(VK_EXT_DEVICE_FAULT_EXTENSION_NAME, 1)) {
        VkPhysicalDeviceFaultFeaturesEXT * deviceFaultFeature;
        deviceFaultFeature = (VkPhysicalDeviceFaultFeaturesEXT*)malloc(
                sizeof(VkPhysicalDeviceFaultFeaturesEXT));
        deviceFaultFeature->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT;
        deviceFaultFeature->pNext = nullptr;
        *tailPNext = deviceFaultFeature;
        tailPNext = &deviceFaultFeature->pNext;
    }

    fGetPhysicalDeviceFeatures2(fPhysicalDevice, &feature);
    feature.features.robustBufferAccess = VK_FALSE;

    float queuePriorities = 0.0;
    void* queueNextPtr = nullptr;
    VkDeviceQueueGlobalPriorityCreateInfoEXT queuePriorityCreateInfo;
    if (fContextPriority != 0 &&
        grExtensions.hasExtension(VK_EXT_GLOBAL_PRIORITY_EXTENSION_NAME, 2)) {
        memset(&queuePriorityCreateInfo, 0, sizeof(VkDeviceQueueGlobalPriorityCreateInfoEXT));
        queuePriorityCreateInfo.pNext = nullptr;
        switch(fContextPriority) {
            case EGL_CONTEXT_PRIORITY_LOW_IMG:
                queuePriorityCreateInfo.globalPriority = VK_QUEUE_GLOBAL_PRIORITY_LOW_EXT;
                break;
            case EGL_CONTEXT_PRIORITY_MEDIUM_IMG:
                queuePriorityCreateInfo.globalPriority = VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT;
                break;
            case EGL_CONTEXT_PRIORITY_HIGH_IMG:
                queuePriorityCreateInfo.globalPriority = VK_QUEUE_GLOBAL_PRIORITY_HIGH_EXT;
                break;
            default:
                assert(false);
        }
        queueNextPtr = &queuePriorityCreateInfo;
    }

    const VkDeviceQueueCreateInfo queueCreateInfo[2] = {
            {
                    VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    queueNextPtr,
                    0,
                    fGraphicQueueIndex,
                    1,
                    &queuePriorities,
            },
            {
                    VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    queueNextPtr,
                    0,
                    fPresentQueueIndex,
                    1,
                    &queuePriorities,
            }
    };
    uint32_t queueInfoCnt = (fPresentQueueIndex != fGraphicQueueIndex) ? 2 : 1;
    const VkDeviceCreateInfo deviceCreateInfo = {
            VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            &feature,
            0,
            queueInfoCnt,
            queueCreateInfo,
            0,
            nullptr,
            (uint32_t)fDeviceExtensions.size(),
            fDeviceExtensions.data(),
            nullptr
    };

    err = fCreateDevice(fPhysicalDevice, &deviceCreateInfo, nullptr, &fDevice);
    assert(err == VK_SUCCESS);

    //获取驱动提供的专用于Blur的输出

    GET_DEVICE_PROC(AllocateCommandBuffers);
    GET_DEVICE_PROC(BeginCommandBuffer);
    GET_DEVICE_PROC(CmdPipelineBarrier);
    GET_DEVICE_PROC(CreateCommandPool);
    GET_DEVICE_PROC(CreateFence);
    GET_DEVICE_PROC(CreateSemaphore);
    GET_DEVICE_PROC(DestroyCommandPool);
    GET_DEVICE_PROC(DestroyDevice);
    GET_DEVICE_PROC(DestroyFence);
    GET_DEVICE_PROC(DestroySemaphore);
    GET_DEVICE_PROC(DeviceWaitIdle);
    GET_DEVICE_PROC(EndCommandBuffer);
    GET_DEVICE_PROC(FreeCommandBuffers);
    GET_DEVICE_PROC(GetDeviceQueue);
    GET_DEVICE_PROC(GetSemaphoreFdKHR);
    GET_DEVICE_PROC(ImportSemaphoreFdKHR);
    GET_DEVICE_PROC(QueueSubmit);
    GET_DEVICE_PROC(QueueWaitIdle);
    GET_DEVICE_PROC(ResetCommandBuffer);
    GET_DEVICE_PROC(ResetFences);
    GET_DEVICE_PROC(WaitForFences);
    GET_DEVICE_PROC(CreateSwapchainKHR);
    GET_DEVICE_PROC(GetSwapchainImagesKHR);
    GET_DEVICE_PROC(DestroySwapchainKHR);
    GET_DEVICE_PROC(QueuePresentKHR);
    GET_DEVICE_PROC(AcquireNextImageKHR);
    GET_DEVICE_PROC(GetBufferMemoryRequirements2);

    fGetDeviceQueue(fDevice, fGraphicQueueIndex, 0, &fGraphicQueue);
    fGetDeviceQueue(fDevice, fPresentQueueIndex, 0, &fPresentQueue);
}

void onVkDeviceFault(const std::string& contextLabel, const std::string& description,
                     const std::vector<VkDeviceFaultAddressInfoEXT>& addressInfos,
                     const std::vector<VkDeviceFaultVendorInfoEXT>& vendorInfos,
                     const std::vector<std::byte>& vendorBinaryData) {
    // The final crash string should contain as much differentiating info as possible, up to 1024
    // bytes. As this final message is constructed, the same information is also dumped to the logs
    // but in a more verbose format. Building the crash string is unsightly, so the clearer logging
    // statement is always placed first to give context.
    aout << "VK_ERROR_DEVICE_LOST (" << contextLabel.c_str() << "): " << description.c_str();
    std::stringstream crashMsg;
    crashMsg << "VK_ERROR_DEVICE_LOST (" << contextLabel;

    if (!addressInfos.empty()) {
        aout << addressInfos.size() << "VkDeviceFaultAddressInfoEXT";
        crashMsg << ", " << addressInfos.size() << " address info (";
        for (VkDeviceFaultAddressInfoEXT addressInfo : addressInfos) {
            aout << " addressType:       " << (int)addressInfo.addressType;
            aout <<"   reportedAddress:   " << (int)addressInfo.reportedAddress;
            aout <<"  addressPrecision: " << addressInfo.addressPrecision;
            crashMsg << addressInfo.addressType << ":"
                     << addressInfo.reportedAddress << ":"
                     << addressInfo.addressPrecision << ", ";
        }
        crashMsg.seekp(-2, crashMsg.cur);  // Move back to overwrite trailing ", "
        crashMsg << ")";
    }

    if (!vendorInfos.empty()) {
        aout << vendorInfos.size() << " VkDeviceFaultVendorInfoEXT:";
        crashMsg << ", " << vendorInfos.size() << " vendor info (";
        for (VkDeviceFaultVendorInfoEXT vendorInfo : vendorInfos) {
            aout << " description:      " << vendorInfo.description;
            aout << "  vendorFaultCode: " << vendorInfo.vendorFaultCode;
            aout << "  vendorFaultData: " << vendorInfo.vendorFaultData;
            // Omit descriptions for individual vendor info structs in the crash string, as the
            // fault code and fault data fields should be enough for clustering, and the verbosity
            // isn't worth it. Additionally, vendors may just set the general description field of
            // the overall fault to the description of the first element in this list, and that
            // overall description will be placed at the end of the crash string.
            crashMsg << vendorInfo.vendorFaultCode << ":"
                     << vendorInfo.vendorFaultData << ", ";
        }
        crashMsg.seekp(-2, crashMsg.cur);  // Move back to overwrite trailing ", "
        crashMsg << ")";
    }

    if (!vendorBinaryData.empty()) {
        // TODO: b/322830575 - Log in base64, or dump directly to a file that gets put in bugreports
        aout << vendorBinaryData.size() << " bytes of vendor-specific binary data (please notify Android's Core Graphics"
                                           "Stack team if you observe this message).";
        crashMsg << ", " << vendorBinaryData.size() << " bytes binary";
    }

    crashMsg << "): " << description;
    aout <<  crashMsg.str().c_str();
}

void deviceLostProc(void* callbackContext, const std::string& description,
                    const std::vector<VkDeviceFaultAddressInfoEXT>& addressInfos,
                    const std::vector<VkDeviceFaultVendorInfoEXT>& vendorInfos,
                    const std::vector<std::byte>& vendorBinaryData) {
    onVkDeviceFault("RenderThread", description, addressInfos, vendorInfos, vendorBinaryData);
}

sk_sp<GrDirectContext> VulkanManager::createContext(GrContextOptions& options) {

    auto getProc = [](const char* proc_name, VkInstance instance, VkDevice device) {
        if (device != VK_NULL_HANDLE) {
            return vkGetDeviceProcAddr(device, proc_name);
        }
        return vkGetInstanceProcAddr(instance, proc_name);
    };

    GrVkBackendContext backendContext;
    backendContext.fInstance = fInstance;
    backendContext.fPhysicalDevice = fPhysicalDevice;
    backendContext.fDevice = fDevice;
    backendContext.fQueue = fGraphicQueue;
    backendContext.fGraphicsQueueIndex = fGraphicQueueIndex;
    backendContext.fMaxAPIVersion = fAPIVersion;
    backendContext.fVkExtensions = &fExtensions;
    backendContext.fDeviceFeatures2 = &fPhysicalDeviceFeatures2;
    backendContext.fGetProc = std::move(getProc);
    backendContext.fDeviceLostContext = nullptr;
    backendContext.fDeviceLostProc = deviceLostProc;

    return GrDirectContexts::MakeVulkan(backendContext, options);
}

VulkanSurface* VulkanManager::createSurface(ANativeWindow* window,
                                            ColorMode colorMode,
                                            sk_sp<SkColorSpace> surfaceColorSpace,
                                            SkColorType surfaceColorType,
                                            GrDirectContext* grContext,
                                            uint32_t extraBuffers) {
    assert(hasVkContext() && "Invalid vulkan device");
    if (!window) {
        return nullptr;
    }


    return VulkanSurface::Create(window, colorMode, surfaceColorType,
                                 surfaceColorSpace, grContext, extraBuffers);
}

void VulkanManager::registerSurface(VkSurfaceKHR surface) {
    std::lock_guard<std::mutex> lock(fSurfaceLock);
    fAliveSurfaces.insert(surface);
}

void VulkanManager::unregisterSurface(VkSurfaceKHR surface) {
    std::lock_guard<std::mutex> lock(fSurfaceLock);
    fAliveSurfaces.erase(surface);
}

bool VulkanManager::hasAliveSurface() {
    std::lock_guard<std::mutex> lock(fSurfaceLock);
    return !fAliveSurfaces.empty();
}
