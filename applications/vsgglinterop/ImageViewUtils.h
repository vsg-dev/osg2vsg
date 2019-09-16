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
using HandleType = HANDLE;
const VkExternalMemoryHandleTypeFlagBits memoryHandleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
using HandleType = int;
const VkExternalMemoryHandleTypeFlagBits memoryHandleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif

namespace vsg
{
    struct VsgImage
    {
        vsg::ref_ptr<vsg::ImageView> imageView;
        vsg::ref_ptr<vsg::DeviceMemory> imageMemory;

        VkImageCreateInfo createInfo;
        VkImageAspectFlagBits aspectFlags;
        VkImageLayout targetImageLayout;

        // for shared image
        HandleType memoryHandle;
        uint32_t byteSize;
        bool dedicated;

    };

    extern VkImageCreateInfo createImageCreateInfo(VkExtent2D extents, VkFormat format, VkImageUsageFlagBits usage = VK_IMAGE_USAGE_SAMPLED_BIT, VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL);

    extern VsgImage createImageView(vsg::Device* device, const VkImageCreateInfo& imageCreateInfo, VkImageAspectFlagBits aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout targetImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    extern VsgImage createSharedMemoryImageView(Context& context, const VkImageCreateInfo& imageCreateInfo, VkImageAspectFlagBits aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout targetImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
