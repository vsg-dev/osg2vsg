/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <osg2vsg/ImageUtils.h>

#include <vsg/vk/CommandBuffer.h>

#include <vsg/core/Array2D.h>
#include <vsg/core/Array3D.h>

namespace osg2vsg
{


VkFormat convertGLImageFormatToVulkan(GLenum dataType, GLenum pixelFormat)
{
    using GLtoVkFormatMap = std::map<std::pair<GLenum, GLenum>, VkFormat>;
    static GLtoVkFormatMap s_GLtoVkFormatMap = {
        {{GL_UNSIGNED_BYTE, GL_ALPHA}, VK_FORMAT_R8_UNORM},
        {{GL_UNSIGNED_BYTE, GL_LUMINANCE}, VK_FORMAT_R8_UNORM},
        {{GL_UNSIGNED_BYTE, GL_LUMINANCE_ALPHA}, VK_FORMAT_R8G8_UNORM},
        {{GL_UNSIGNED_BYTE, GL_RGB}, VK_FORMAT_R8G8B8_UNORM},
        {{GL_UNSIGNED_BYTE, GL_RGBA}, VK_FORMAT_R8G8B8A8_UNORM}
    };

    auto itr = s_GLtoVkFormatMap.find({dataType,pixelFormat});
    if (itr!=s_GLtoVkFormatMap.end())
    {
        std::cout<<"convertGLImageFormatToVulkan("<<dataType<<", "<<pixelFormat<<") vkFormat="<<itr->second<<std::endl;
        return itr->second;
    }
    else
    {
        std::cout<<"convertGLImageFormatToVulkan("<<dataType<<", "<<pixelFormat<<") no match found."<<std::endl;
        return VK_FORMAT_UNDEFINED;
    }
}

struct WriteRow : public osg::CastAndScaleToFloatOperation
{
    WriteRow(unsigned char* ptr) : _ptr(ptr) {}
    unsigned char* _ptr;

    inline void luminance(float l) { rgba(l, l, l, 1.0f); }
    inline void alpha(float a) { rgba(1.0f, 1.0f, 1.0f, a); }
    inline void luminance_alpha(float l,float a) { rgba(l, l, l, a); }
    inline void rgb(float r,float g,float b) { rgba(r, g, b, 1.0f); }
    inline void rgba(float r,float g,float b,float a)
    {
        (*_ptr++) = static_cast<unsigned char>(r*255.0);
        (*_ptr++) = static_cast<unsigned char>(g*255.0);
        (*_ptr++) = static_cast<unsigned char>(b*255.0);
        (*_ptr++) = static_cast<unsigned char>(a*255.0);
    }
};

osg::ref_ptr<osg::Image> formatImageToRGBA(const osg::Image* image)
{
    osg::ref_ptr<osg::Image> new_image( new osg::Image);
    new_image->allocateImage(image->s(), image->t(), image->r(), GL_RGBA, GL_UNSIGNED_BYTE);

    // need to copy pixels from image to new_image;
    for(int r=0;r<image->r();++r)
    {
        for(int t=0;t<image->t();++t)
        {
            WriteRow operation(new_image->data(0, t, r));
            osg::readRow(image->s(), image->getPixelFormat(), image->getDataType(), image->data(0,t,r), operation);
        }
    }

    return new_image;
}


vsg::ref_ptr<vsg::Data> createWhiteTexture()
{
    vsg::ref_ptr<vsg::vec4Array2D> vsg_data(new vsg::vec4Array2D(1,1));
    vsg_data->setFormat(VK_FORMAT_R32G32B32A32_SFLOAT);
    for(auto& color : *vsg_data)
    {
        color = vsg::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    return vsg_data;
}

vsg::ref_ptr<vsg::Data> convertCompressedImageToVsg(const osg::Image* image)
{
    uint32_t blockSize = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    vsg::Data::Layout layout;
    switch(image->getPixelFormat())
    {
        case(GL_COMPRESSED_ALPHA_ARB):
        case(GL_COMPRESSED_INTENSITY_ARB):
        case(GL_COMPRESSED_LUMINANCE_ALPHA_ARB):
        case(GL_COMPRESSED_LUMINANCE_ARB):
        case(GL_COMPRESSED_RGBA_ARB):
        case(GL_COMPRESSED_RGB_ARB):
            break;
        case(GL_COMPRESSED_RGB_S3TC_DXT1_EXT):
            blockSize = 64;
            layout.blockWidth = 4;
            layout.blockHeight = 4;
            format = VK_FORMAT_BC1_RGB_UNORM_BLOCK;
            break;
        case(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT):
            blockSize = 64;
            layout.blockWidth = 4;
            layout.blockHeight = 4;
            format = VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
            break;
        case(GL_COMPRESSED_RGBA_S3TC_DXT3_EXT):
            blockSize = 128;
            layout.blockWidth = 4;
            layout.blockHeight = 4;
            format = VK_FORMAT_BC2_UNORM_BLOCK;
            break;
        case(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT):
            blockSize = 128;
            layout.blockWidth = 4;
            layout.blockHeight = 4;
            format = VK_FORMAT_BC3_UNORM_BLOCK;
            break;
        case(GL_COMPRESSED_SIGNED_RED_RGTC1_EXT):
        case(GL_COMPRESSED_RED_RGTC1_EXT):
        case(GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT):
        case(GL_COMPRESSED_RED_GREEN_RGTC2_EXT):
        case(GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG):
        case(GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG):
        case(GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG):
        case(GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG):
        case(GL_ETC1_RGB8_OES):
        case(GL_COMPRESSED_RGB8_ETC2):
        case(GL_COMPRESSED_SRGB8_ETC2):
        case(GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2):
        case(GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2):
        case(GL_COMPRESSED_RGBA8_ETC2_EAC):
        case(GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC):
        case(GL_COMPRESSED_R11_EAC):
        case(GL_COMPRESSED_SIGNED_R11_EAC):
        case(GL_COMPRESSED_RG11_EAC):
        case(GL_COMPRESSED_SIGNED_RG11_EAC):
        case (GL_COMPRESSED_RGBA_ASTC_4x4_KHR) :
        case (GL_COMPRESSED_RGBA_ASTC_5x4_KHR) :
        case (GL_COMPRESSED_RGBA_ASTC_5x5_KHR) :
        case (GL_COMPRESSED_RGBA_ASTC_6x5_KHR) :
        case (GL_COMPRESSED_RGBA_ASTC_6x6_KHR) :
        case (GL_COMPRESSED_RGBA_ASTC_8x5_KHR) :
        case (GL_COMPRESSED_RGBA_ASTC_8x6_KHR) :
        case (GL_COMPRESSED_RGBA_ASTC_8x8_KHR) :
        case (GL_COMPRESSED_RGBA_ASTC_10x5_KHR) :
        case (GL_COMPRESSED_RGBA_ASTC_10x6_KHR) :
        case (GL_COMPRESSED_RGBA_ASTC_10x8_KHR) :
        case (GL_COMPRESSED_RGBA_ASTC_10x10_KHR) :
        case (GL_COMPRESSED_RGBA_ASTC_12x10_KHR) :
        case (GL_COMPRESSED_RGBA_ASTC_12x12_KHR) :
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR) :
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR) :
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR) :
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR) :
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR) :
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR) :
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR) :
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR) :
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR) :
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR) :
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR) :
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR) :
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR) :
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR) :
            break;
        default:
            break;
    }

    if (blockSize==0)
    {
        std::cout<<"Compressed format not supported, falling back to white texture."<<std::endl;
        return createWhiteTexture();
    }

    // stop the OSG image from deleting the image data as we are passing this on to the vsg Data object
    auto size = image->getTotalSizeInBytesIncludingMipmaps();
    uint8_t* data = new uint8_t[size];
    memcpy(data, image->data(), size);

    layout.maxNumMipmaps = image->getNumMipmapLevels();

    uint32_t width = image->s() / layout.blockWidth;
    uint32_t height = image->t() / layout.blockHeight;
    uint32_t depth = image->r() / layout.blockDepth;

    vsg::ref_ptr<vsg::Data> vsg_data;
    if (blockSize==64)
    {
        if (image->r()==1)
        {
            vsg_data = new vsg::block64Array2D(width, height, reinterpret_cast<vsg::block64*>(data));
        }
        else
        {
            vsg_data = new vsg::block64Array3D(width, height, depth, reinterpret_cast<vsg::block64*>(data));
        }
    }
    else
    {
        if (image->r()==1)
        {
            vsg_data = new vsg::block128Array2D(width, height, reinterpret_cast<vsg::block128*>(data));
        }
        else
        {
            vsg_data = new vsg::block128Array3D(width, height, depth, reinterpret_cast<vsg::block128*>(data));
        }
    }

    vsg_data->setFormat(format);
    vsg_data->setLayout(layout);

    return vsg_data;
}

vsg::ref_ptr<vsg::Data> convertToVsg(const osg::Image* image)
{
    if (!image)
    {
        return createWhiteTexture();
    }


    if (image->isCompressed())
    {
        return convertCompressedImageToVsg(image);
    }

    osg::ref_ptr<osg::Image> new_image = formatImageToRGBA(image);

    // we want to pass ownership of the new_image data onto th vsg_image so reset the allocation mode on the image to prevent deletetion.
    new_image->setAllocationMode(osg::Image::NO_DELETE);

    vsg::ref_ptr<vsg::Data> vsg_data;

    vsg::Data::Layout layout;
    layout.maxNumMipmaps = image->getNumMipmapLevels();

    if (new_image->r()==1)
    {
        vsg_data = new vsg::ubvec4Array2D(new_image->s(), new_image->t(), reinterpret_cast<vsg::ubvec4*>(new_image->data()));
    }
    else
    {
        vsg_data = new vsg::ubvec4Array3D(new_image->s(), new_image->t(), new_image->r(), reinterpret_cast<vsg::ubvec4*>(new_image->data()));
    }

    vsg_data->setFormat(VK_FORMAT_R8G8B8A8_UNORM);
    vsg_data->setLayout(layout);

    return vsg_data;
}


vsg::ImageData readImageFile(vsg::Device* device, vsg::CommandPool* commandPool, VkQueue graphicsQueue, const std::string& filename)
{
    osg::ref_ptr<osg::Image> osg_image = osgDB::readImageFile(filename);
    if (!osg_image)
    {
        return vsg::ImageData();
    }

    if(osg_image->getPixelFormat()!=GL_RGBA || osg_image->getDataType()!=GL_UNSIGNED_BYTE)
    {
        std::cout<<"Reformating osg::Image to GL_RGBA, before = "<<osg_image->getPixelFormat()<<std::endl;
        osg_image = osg2vsg::formatImageToRGBA(osg_image);
        std::cout<<"Reformating osg::Image to GL_RGBA, after = "<<osg_image->getPixelFormat()<<", RGBA="<<GL_RGBA<<std::endl;
    }

    VkDeviceSize imageTotalSize = osg_image->getTotalSizeInBytesIncludingMipmaps();

    vsg::ref_ptr<vsg::Buffer> imageStagingBuffer = vsg::Buffer::create(device, imageTotalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE);
    vsg::ref_ptr<vsg::DeviceMemory> imageStagingMemory = vsg::DeviceMemory::create(device, imageStagingBuffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    imageStagingBuffer->bind(imageStagingMemory, 0);

    // copy image data to staging memory
    imageStagingMemory->copy(0, imageTotalSize, osg_image->data());

    std::cout<<"Creating imageStagingBuffer and memorory size = "<<imageTotalSize<<std::endl;


    std::cout<<"VK_FORMAT_R8G8B8A8_UNORM= "<<VK_FORMAT_R8G8B8A8_UNORM<<std::endl;
    std::cout<<"  osg2vsg : "<<osg2vsg::convertGLImageFormatToVulkan(osg_image->getDataType(), osg_image->getPixelFormat())<<std::endl;

    VkImageType imageType = osg_image->r()>1 ? VK_IMAGE_TYPE_3D : (osg_image->t()>1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_1D);;
    VkImageViewType imageViewType = osg_image->r() > 1 ? VK_IMAGE_VIEW_TYPE_3D : (osg_image->t() > 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_1D);

    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = imageType;
    imageCreateInfo.extent.width = osg_image->s();
    imageCreateInfo.extent.height = osg_image->t();
    imageCreateInfo.extent.depth = osg_image->r();
    imageCreateInfo.mipLevels = osg_image->getNumMipmapLevels();
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.format = osg2vsg::convertGLImageFormatToVulkan(osg_image->getDataType(), osg_image->getPixelFormat());
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vsg::ref_ptr<vsg::Image> textureImage = vsg::Image::create(device, imageCreateInfo);
    if (!textureImage)
    {
        return vsg::ImageData();
    }

    vsg::ref_ptr<vsg::DeviceMemory> textureImageDeviceMemory = vsg::DeviceMemory::create(device, textureImage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!textureImageDeviceMemory)
    {
        return vsg::ImageData();
    }

    textureImage->bind(textureImageDeviceMemory, 0);


    vsg::dispatchCommandsToQueue(device, commandPool, graphicsQueue, [&](VkCommandBuffer commandBuffer)
    {

        std::cout<<"Need to dispatch VkCmd's to "<<commandBuffer<<std::endl;

        vsg::ImageMemoryBarrier preCopyImageMemoryBarrier(
                        0, VK_ACCESS_TRANSFER_WRITE_BIT,
                        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        textureImage);

        preCopyImageMemoryBarrier.cmdPiplineBarrier(commandBuffer,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        std::cout<<"CopyBufferToImage()"<<std::endl;

        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {static_cast<uint32_t>(osg_image->s()), static_cast<uint32_t>(osg_image->t()), 1};

        vkCmdCopyBufferToImage(commandBuffer, *imageStagingBuffer, *textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        std::cout<<"Post CopyBufferToImage()"<<std::endl;

        vsg::ImageMemoryBarrier postCopyImageMemoryBarrier(
                        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        textureImage);

        postCopyImageMemoryBarrier.cmdPiplineBarrier(commandBuffer,
                        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        std::cout<<"Post postCopyImageMemoryBarrier()"<<std::endl;
    });

    // clean up staging buffer
    imageStagingBuffer = 0;
    imageStagingMemory = 0;

    // delete osg_image as it's no longer required.
    osg_image = 0;

    vsg::ref_ptr<vsg::Sampler> textureSampler = vsg::Sampler::create(device);
    vsg::ref_ptr<vsg::ImageView> textureImageView = vsg::ImageView::create(device, textureImage, imageViewType, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

    std::cout<<"textureSampler = "<<textureSampler.get()<<std::endl;
    std::cout<<"textureImageView = "<<textureImageView.get()<<std::endl;

    return vsg::ImageData(textureSampler, textureImageView, VK_IMAGE_LAYOUT_UNDEFINED);
}


} // end of namespace osg2cpp
