#pragma once

#define NOMINMAX

#if defined(WIN32)
#    define VK_USE_PLATFORM_WIN32_KHR
#elif defined(ANDROID)
#    define VK_USE_PLATFORM_ANDROID_KHR
#elif defined(APPLE)
#    define VK_USE_PLATFORM_MACOS_MVK
#elif defined(UNIX)
#    define VK_USE_PLATFORM_XCB_KHR
#endif

#include <vulkan/vulkan.h>

#include <vsg/all.h>

#if WIN32
#include <Windows.h>
#endif

namespace vsg
{
    class SharedSemaphore : public Inherit<Semaphore, SharedSemaphore>
    {
    public:
        SharedSemaphore(VkSemaphore Semaphore, Device* device, AllocationCallbacks* allocator = nullptr) :
            Inherit(Semaphore, device, allocator)
        {}

        using Result = vsg::Result<SharedSemaphore, VkResult, VK_SUCCESS>;
        static Result create(Device* device, void* pNextCreateInfo = nullptr, AllocationCallbacks* allocator = nullptr)
        {
            if (!device)
            {
                return Result("Error: vsg::SharedSemaphore::create(...) failed to create command pool, undefined Device.", VK_ERROR_INVALID_EXTERNAL_HANDLE);
            }

            VkSemaphoreCreateInfo semaphoreInfo = {};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            semaphoreInfo.pNext = pNextCreateInfo;

            VkSemaphore semaphore;
            VkResult result = vkCreateSemaphore(*device, &semaphoreInfo, allocator, &semaphore);
            if (result == VK_SUCCESS)
            {
                return Result(new SharedSemaphore(semaphore, device, allocator));
            }
            else
            {
                return Result("Error: Failed to create SharedSemaphore.", result);
            }
        }

#ifdef WIN32
        typedef HANDLE HandleType;
        static const VkExternalSemaphoreHandleTypeFlagBits kSemaphoreHandleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
        typedef int HandleType;
        static const VkExternalSemaphoreHandleTypeFlagBits kSemaphoreHandleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif

        HandleType _handle;
    };

    extern ref_ptr<SharedSemaphore> createSharedSemaphore(Device* device);
}

