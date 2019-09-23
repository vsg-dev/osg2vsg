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
    class SharedImage : public Inherit<Image, SharedImage>
    {
    public:
        SharedImage(VkImage image, Device* device, AllocationCallbacks* allocator = nullptr) :
            Inherit(image, device, allocator)
        {
        }

        using Result = vsg::Result<SharedImage, VkResult, VK_SUCCESS>;
        static Result create(Device* device, const VkImageCreateInfo& createImageInfo, AllocationCallbacks* allocator = nullptr)
        {
            if (!device)
            {
                return Result("Error: vsg::SharedImage::create(...) failed to create vkImage, undefined Device.", VK_ERROR_INVALID_EXTERNAL_HANDLE);
            }

            VkImage image;
            VkResult result = vkCreateImage(*device, &createImageInfo, allocator, &image);
            if (result == VK_SUCCESS)
            {
                return Result(new SharedImage(image, device, allocator));
            }
            else
            {
                return Result("Error: Failed to create vkImage.", result);
            }
        }

#if WIN32
        using HandleType = HANDLE;
        static const VkExternalMemoryHandleTypeFlagBits kMemoryHandleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
        using HandleType = int;
        static const VkExternalMemoryHandleTypeFlagBits kMemoryHandleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif

        HandleType _memoryHandle;
        uint32_t _byteSize;
        bool _dedicated;
    };

    extern VkImageCreateInfo createImageCreateInfo(VkExtent2D extents, VkFormat format, VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT, VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL);

    extern ImageData createImageView(Context& context, const VkImageCreateInfo& imageCreateInfo, VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT, bool shared = false, VkImageLayout targetImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    extern ref_ptr<SharedImage> createSharedMemoryImage(Context& context, const VkImageCreateInfo& imageCreateInfo);
}
