/* <editor-fold desc="MIT License">

Copyright(c) 2019 Thomas Hogarth

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#pragma once

#include <vsg/vk/Descriptor.h>
#include <vsg/vk/ImageData.h>

#if WIN32
#include <Windows.h>
#endif

namespace vsg
{
    class SharedDescriptorImage : public Inherit<Descriptor, SharedDescriptorImage>
    {
    public:
        SharedDescriptorImage(ref_ptr<Sampler> sampler, VkExtent2D dimensions, VkFormat format, VkImageTiling tiling, uint32_t dstBinding = 0, uint32_t dstArrayElement = 0, VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

#if WIN32
        using HandleType = HANDLE;
        const VkExternalMemoryHandleTypeFlagBits memoryHandleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
        using HandleType = int;
        const VkExternalMemoryHandleTypeFlagBits memoryHandleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif
        const VkExtent2D& dimensions() { return _dimensions; }
        const VkFormat format() { return _format; }
        const VkImageTiling tiling() { return _tiling; }
        const bool dedicated() { return _dedicated; }

        const HandleType handle() { return _memoryHandle; }
        ImageView* imageView() { return _imageData._imageView; }

        const uint32_t byteSize() { return _byteSize; }

        void compile(Context& context) override;

        bool assignTo(VkWriteDescriptorSet& wds, VkDescriptorSet descriptorSet) const override;

        uint32_t getNumDescriptors() const override;

        vsg::ImageData createSharedMemoryImageData(Context& context, Sampler* sampler, VkImageLayout targetImageLayout);

    protected:
        virtual ~SharedDescriptorImage();

        VkExtent2D _dimensions { 0, 0 };
        VkFormat _format { VK_FORMAT_UNDEFINED };
        VkImageTiling _tiling { VK_IMAGE_TILING_LINEAR };
        bool _dedicated { false };

        HandleType _memoryHandle { 0 };

        vsg::ref_ptr<Sampler> _sampler;

        // populated by compile()
        ImageData _imageData;
        VkDescriptorImageInfo _imageInfo;

        uint32_t _byteSize;
    };
    VSG_type_name(vsg::SharedDescriptorImage)
}

