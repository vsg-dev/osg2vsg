#include "ImageViewUtils.h"

using namespace vsg;

VkImageCreateInfo vsg::createImageCreateInfo(VkExtent2D extents, VkFormat format, VkImageUsageFlagBits usage, VkImageTiling tiling)
{
    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.extent.width = extents.width;
    imageCreateInfo.extent.height = extents.height;
    imageCreateInfo.format = format;
    imageCreateInfo.usage = usage;
    imageCreateInfo.extent.depth = 1;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.tiling = tiling;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.pNext = nullptr;

    return imageCreateInfo;
}

VsgImage vsg::createImageView(vsg::Device* device, const VkImageCreateInfo& imageCreateInfo, VkImageAspectFlagBits aspectFlags, VkImageLayout targetImageLayout)
{
    vsg::ref_ptr<vsg::Image> image = vsg::Image::create(device, imageCreateInfo);
    vsg::ref_ptr<vsg::DeviceMemory> imageMemory = vsg::DeviceMemory::create(device, image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vkBindImageMemory(*device, *image, *imageMemory, 0);

    vsg::ImageData imagedata;

    VsgImage result;
    result.imageView = vsg::ImageView::create(device, image, VK_IMAGE_VIEW_TYPE_2D, imageCreateInfo.format, aspectFlags);
    result.imageMemory = imageMemory;
    result.createInfo = imageCreateInfo;
    result.aspectFlags = aspectFlags;
    result.targetImageLayout = targetImageLayout;
    return result;
}

VsgImage vsg::createSharedMemoryImageView(Context& context, const VkImageCreateInfo& imageCreateInfo, VkImageAspectFlagBits aspectFlags, VkImageLayout targetImageLayout)
{
    Device* device = context.device;

    // query chain
    VkPhysicalDeviceImageFormatInfo2 imageFormatInfo;
    imageFormatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
    imageFormatInfo.format = imageCreateInfo.format;
    imageFormatInfo.type = VK_IMAGE_TYPE_2D;
    imageFormatInfo.tiling = imageCreateInfo.tiling;
    imageFormatInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VkPhysicalDeviceExternalImageFormatInfo externalImageFormatInfo;
    externalImageFormatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
    externalImageFormatInfo.handleType = memoryHandleType;
    externalImageFormatInfo.pNext = nullptr;

    imageFormatInfo.pNext = &externalImageFormatInfo;

    // results chain
    VkImageFormatProperties2 imageFormatProperties2;
    imageFormatProperties2.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;

    VkExternalImageFormatProperties externalImageFormatProperties;
    externalImageFormatProperties.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
    externalImageFormatProperties.pNext = nullptr;

    imageFormatProperties2.pNext = &externalImageFormatProperties;

    vkGetPhysicalDeviceImageFormatProperties2(device->getPhysicalDevice()->getPhysicalDevice(), &imageFormatInfo, &imageFormatProperties2);

    VkImageFormatProperties imageFormatProperties = imageFormatProperties2.imageFormatProperties;

    bool dedicated = false;
    if (externalImageFormatProperties.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT)
    {
        dedicated = true;
    }

    ref_ptr<Image> textureImage = Image::create(device, imageCreateInfo);
    if (!textureImage)
    {
        return VsgImage();
    }

    // Always add the export info to the memory allocation chain
    VkExportMemoryAllocateInfo exportAllocInfo;
    exportAllocInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    exportAllocInfo.handleTypes = memoryHandleType;
    exportAllocInfo.pNext = nullptr;

    VkMemoryDedicatedAllocateInfo dedicatedMemAllocInfo;
    if (dedicated)
    {
        // Potentially add the dedicated memory allocation
        dedicatedMemAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
        dedicatedMemAllocInfo.image = textureImage->image();
        dedicatedMemAllocInfo.buffer = VK_NULL_HANDLE;
        dedicatedMemAllocInfo.pNext = nullptr;
        exportAllocInfo.pNext = &dedicatedMemAllocInfo;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(*device, *textureImage, &memRequirements);

    auto[deviceMemory, offset] = context.deviceMemoryBufferPools.reserveMemory(memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &exportAllocInfo);

    if (!deviceMemory)
    {
        std::cout << "Warning: vsg::transferImageData() Failed allocate memory to reserve slot" << std::endl;
        return VsgImage();
    }

    textureImage->bind(deviceMemory, offset);

#if WIN32
    VkMemoryGetWin32HandleInfoKHR getHandleInfo;
    getHandleInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
#else
    VkMemoryGetFdInfoKHR getHandleInfo;
    getHandleInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
#endif
    getHandleInfo.handleType = memoryHandleType;
    getHandleInfo.memory = *deviceMemory;
    getHandleInfo.pNext = nullptr;

    HandleType memoryHandle = 0;

    auto vkGetMemoryWin32HandleKHR2 = PFN_vkGetMemoryWin32HandleKHR(vkGetDeviceProcAddr(device->getDevice(), "vkGetMemoryWin32HandleKHR"));
    vkGetMemoryWin32HandleKHR2(device->getDevice(), &getHandleInfo, &memoryHandle);

    VkImageViewCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = *textureImage;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = imageCreateInfo.format;
    createInfo.subresourceRange.aspectMask = aspectFlags;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;
    createInfo.pNext = nullptr;

    VsgImage result;
    result.imageView = ImageView::create(device, createInfo);
    result.imageView->setImage(textureImage);
    result.createInfo = imageCreateInfo;
    result.aspectFlags = aspectFlags;
    result.targetImageLayout = targetImageLayout;

    result.memoryHandle = memoryHandle;
    result.byteSize = static_cast<uint32_t>(memRequirements.size);
    result.dedicated = dedicated;

    return result;
}