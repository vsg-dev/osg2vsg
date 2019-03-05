#include <vsg/vk/BufferData.h>
#include <vsg/vk/Descriptor.h>
#include <vsg/vk/DescriptorSet.h>
#include <vsg/vk/DescriptorPool.h>
#include <vsg/vk/BindVertexBuffers.h>
#include <vsg/vk/BindIndexBuffer.h>
#include <vsg/vk/Draw.h>

#include <vsg/io/ReaderWriter.h>

#include <vsg/traversals/DispatchTraversal.h>

#include <iostream>

#include <osg2vsg/StateAttributes.h>

using namespace vsg;

#if 0
#define DEBUG_OUTPUT std::cout
#else
#define DEBUG_OUTPUT if (false) std::cout
#endif

/////////////////////////////////////////////////////////////////////////////////////////
//
// Texture
//
Texture::Texture() :
    Inherit(0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
{
    // set default sampler info
    _samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    _samplerInfo.minFilter = VK_FILTER_LINEAR;
    _samplerInfo.magFilter = VK_FILTER_LINEAR;
    _samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    _samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    _samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
#if 1
    // requres Logical device to have deviceFeatures.samplerAnisotropy = VK_TRUE; set when creating the vsg::Device
    _samplerInfo.anisotropyEnable = VK_TRUE;
    _samplerInfo.maxAnisotropy = 16;
#else
    _samplerInfo.anisotropyEnable = VK_FALSE;
    _samplerInfo.maxAnisotropy = 1;
#endif
    _samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    _samplerInfo.unnormalizedCoordinates = VK_FALSE;
    _samplerInfo.compareEnable = VK_FALSE;
    _samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
}

void Texture::compile(Context& context)
{
    ref_ptr<Sampler> sampler = Sampler::create(context.device, _samplerInfo, nullptr);
    vsg::ImageData imageData = vsg::transferImageData(context.device, context.commandPool, context.graphicsQueue, _textureData, sampler);
    if (!imageData.valid())
    {
        DEBUG_OUTPUT<<"Texture not created"<<std::endl;
        return;
    }

    _implementation = vsg::DescriptorImage::create(_dstBinding, _dstArrayElement, _descriptorType, vsg::ImageDataList{imageData});
}

void Texture::assignTo(VkWriteDescriptorSet& wds, VkDescriptorSet descriptorSet) const
{
    if (_implementation) _implementation->assignTo(wds, descriptorSet);
}
