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

    if (image->getOrigin()==osg::Image::BOTTOM_LEFT) layout.origin = 1;

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

    if (image->getOrigin()==osg::Image::BOTTOM_LEFT) layout.origin = 2;

    vsg_data->setFormat(VK_FORMAT_R8G8B8A8_UNORM);
    vsg_data->setLayout(layout);

    return vsg_data;
}

} // end of namespace osg2cpp
