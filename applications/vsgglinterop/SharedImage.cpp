#include "SharedImage.h"

using namespace vsg;

VkImageCreateInfo vsg::createImageCreateInfo(VkExtent2D extents, VkFormat format, VkImageUsageFlags usage, VkImageTiling tiling)
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

ImageData vsg::createImageView(Context& context, const VkImageCreateInfo& imageCreateInfo, VkImageAspectFlags aspectFlags, bool shared, VkImageLayout targetImageLayout)
{
    Device* device = context.device;

    vsg::ref_ptr<vsg::Image> image;

    if (shared)
    {
        image = createSharedMemoryImage(context, imageCreateInfo);
    }
    else
    {
        image = vsg::Image::create(device, imageCreateInfo);

        // get memory requirements
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(*device, *image, &memRequirements);

        // allocate memory with out export memory info extension
        auto[deviceMemory, offset] = context.deviceMemoryBufferPools.reserveMemory(memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (!deviceMemory)
        {
            std::cout << "Warning: Failed allocate memory to reserve slot" << std::endl;
            return ImageData();
        }

        image->bind(deviceMemory, offset);
    }

    ref_ptr<ImageView> imageview = vsg::ImageView::create(device, image, VK_IMAGE_VIEW_TYPE_2D, imageCreateInfo.format, aspectFlags);

    return ImageData(nullptr, imageview, targetImageLayout);
}

ref_ptr<SharedImage> vsg::createSharedMemoryImage(Context& context, const VkImageCreateInfo& imageCreateInfo)
{
    Device* device = context.device;

    // query if this needs to be a dedicated resource

    VkPhysicalDeviceImageFormatInfo2 imageFormatInfo;
    imageFormatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
    imageFormatInfo.format = imageCreateInfo.format;
    imageFormatInfo.type = VK_IMAGE_TYPE_2D;
    imageFormatInfo.tiling = imageCreateInfo.tiling;
    imageFormatInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VkPhysicalDeviceExternalImageFormatInfo externalImageFormatInfo;
    externalImageFormatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
    externalImageFormatInfo.handleType = SharedImage::kMemoryHandleType;
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

    // create the image
    ref_ptr<SharedImage> image = SharedImage::create(device, imageCreateInfo);
    if (!image)
    {
        return image;
    }

    // always add the export info to the memory allocation chain
    VkExportMemoryAllocateInfo exportAllocInfo;
    exportAllocInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    exportAllocInfo.handleTypes = SharedImage::kMemoryHandleType;
    exportAllocInfo.pNext = nullptr;

    VkMemoryDedicatedAllocateInfo dedicatedMemAllocInfo;
    if (dedicated)
    {
        // potentially add the dedicated memory allocation
        dedicatedMemAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
        dedicatedMemAllocInfo.image = image->image();
        dedicatedMemAllocInfo.buffer = VK_NULL_HANDLE;
        dedicatedMemAllocInfo.pNext = nullptr;
        exportAllocInfo.pNext = &dedicatedMemAllocInfo;
    }

    // get memory requirements
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(*device, *image, &memRequirements);

    // allocate memory with out export memory info extension
    auto[deviceMemory, offset] = context.deviceMemoryBufferPools.reserveMemory(memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &exportAllocInfo);

    if (!deviceMemory)
    {
        std::cout << "Warning: Failed allocate memory to reserve slot" << std::endl;
        return image;
    }

    image->bind(deviceMemory, offset);

    // get the memory handle
#if WIN32
    auto vkGetMemoryHandle = PFN_vkGetMemoryWin32HandleKHR(vkGetDeviceProcAddr(device->getDevice(), "vkGetMemoryWin32HandleKHR"));

    VkMemoryGetWin32HandleInfoKHR getHandleInfo;
    getHandleInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
#else
    auto vkGetMemoryHandle = PFN_vkGetMemoryFdKHR(vkGetDeviceProcAddr(device->getDevice(), "vkGetMemoryFdKHR"));

    VkMemoryGetFdInfoKHR getHandleInfo;
    getHandleInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
#endif
    getHandleInfo.handleType = SharedImage::kMemoryHandleType;
    getHandleInfo.memory = *deviceMemory;
    getHandleInfo.pNext = nullptr;

    SharedImage::HandleType memoryHandle = 0;
    vkGetMemoryHandle(device->getDevice(), &getHandleInfo, &memoryHandle);
    
    image->_memoryHandle = memoryHandle;
    image->_byteSize = static_cast<uint32_t>(memRequirements.size);
    image->_dedicated = dedicated;

    return image;
}