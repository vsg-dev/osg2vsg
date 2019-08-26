/* <editor-fold desc="MIT License">

Copyright(c) 2019 Thomas Hogarth

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

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

#include "SharedDescriptorImage.h"

#include <iostream>

#include <vsg/traversals/CompileTraversal.h>
#include <vsg/vk/CommandBuffer.h>

using namespace vsg;

class MoveToTargetLayoutImageDataCommand : public Command
{
public:
    MoveToTargetLayoutImageDataCommand(ImageData imageData) :
        _imageData(imageData) {}

    void dispatch(CommandBuffer& commandBuffer) const override
    {
        ref_ptr<Image> textureImage(_imageData._imageView->getImage());
        VkImageLayout targetImageLayout = _imageData._imageLayout;

        ImageMemoryBarrier postCopyImageMemoryBarrier(
            0, VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, targetImageLayout,
            textureImage);

        postCopyImageMemoryBarrier.cmdPiplineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }

    ImageData _imageData;

protected:
    virtual ~MoveToTargetLayoutImageDataCommand() {}
};

SharedDescriptorImage::SharedDescriptorImage(ref_ptr<Sampler> sampler, VkExtent2D dimensions, VkFormat format, VkImageTiling tiling, uint32_t dstBinding, uint32_t dstArrayElement, VkDescriptorType descriptorType) :
    Inherit(dstBinding, dstArrayElement, descriptorType),
    _dimensions(dimensions),
    _format(format),
    _tiling(tiling),
    _dedicated(false),
    _sampler(sampler),
    _byteSize(0)
{
}


SharedDescriptorImage::~SharedDescriptorImage()
{
}

vsg::ImageData SharedDescriptorImage::createSharedMemoryImageData(Context& context, Sampler* sampler, VkImageLayout targetImageLayout)
{
    Device* device = context.device;

    // query chain
    VkPhysicalDeviceImageFormatInfo2 imageFormatInfo;
    imageFormatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
    imageFormatInfo.format = _format;
    imageFormatInfo.type = VK_IMAGE_TYPE_2D;
    imageFormatInfo.tiling = _tiling;
    imageFormatInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;// VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

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

    if (externalImageFormatProperties.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT)
    {
        _dedicated = true;
    }

    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.extent.width = _dimensions.width;
    imageCreateInfo.extent.height = _dimensions.height;
    imageCreateInfo.extent.depth = 1;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.format = _format;
    imageCreateInfo.tiling = _tiling;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.usage = imageFormatInfo.usage;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.pNext = nullptr;

    ref_ptr<Image> textureImage = Image::create(device, imageCreateInfo);
    if (!textureImage)
    {
        return ImageData();
    }


    // Always add the export info to the memory allocation chain
    VkExportMemoryAllocateInfo exportAllocInfo;
    exportAllocInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    exportAllocInfo.handleTypes = memoryHandleType;
    exportAllocInfo.pNext = nullptr;

    VkMemoryDedicatedAllocateInfo dedicatedMemAllocInfo;
    if (_dedicated)
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
        return ImageData();
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

    auto vkGetMemoryWin32HandleKHR2 = PFN_vkGetMemoryWin32HandleKHR(vkGetDeviceProcAddr(device->getDevice(), "vkGetMemoryWin32HandleKHR"));
    vkGetMemoryWin32HandleKHR2(device->getDevice(), &getHandleInfo, &_memoryHandle);

    _byteSize = memRequirements.size;

    VkImageViewCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = *textureImage;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = imageCreateInfo.format;
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;
    createInfo.pNext = nullptr;

    ref_ptr<ImageView> textureImageView = ImageView::create(device, createInfo);
    if (textureImageView) textureImageView->setImage(textureImage);

    ImageData imageData(sampler, textureImageView, targetImageLayout);

    context.commands.emplace_back(new MoveToTargetLayoutImageDataCommand(imageData));

    return imageData;
}

void SharedDescriptorImage::compile(Context& context)
{
    // check if we have already compiled the imageData.
    if (_byteSize != 0) return;

    if (_dimensions.width != 0 && _dimensions.height != 0)
    {
        if(_sampler != nullptr) _sampler->compile(context);
        _imageData = createSharedMemoryImageData(context, _sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // convert from VSG to Vk

        if (_imageData._sampler)
            _imageInfo.sampler = *(_imageData._sampler);
        else
            _imageInfo.sampler = 0;

        if (_imageData._imageView)
            _imageInfo.imageView = *(_imageData._imageView);
        else
            _imageInfo.imageView = 0;

        _imageInfo.imageLayout = _imageData._imageLayout;
    }
}

bool SharedDescriptorImage::assignTo(VkWriteDescriptorSet& wds, VkDescriptorSet descriptorSet) const
{
    Descriptor::assignTo(wds, descriptorSet);
    wds.descriptorCount = 1;
    wds.pImageInfo = &_imageInfo;
    return true;
}

uint32_t SharedDescriptorImage::getNumDescriptors() const
{
    return 1;
}