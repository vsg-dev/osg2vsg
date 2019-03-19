#pragma once

#include <osg2vsg/Export.h>
#include <vsg/all.h>

#include <osg/Array>
#include <osg/Geometry>
#include <osg/Material>

namespace osg2vsg
{
    enum GeometryAttributes : uint32_t
    {
        VERTEX = 1,
        NORMAL = 2,
        NORMAL_OVERALL = 4,
        TANGENT = 8,
        TANGENT_OVERALL = 16,
        COLOR = 32,
        COLOR_OVERALL = 64,
        TEXCOORD0 = 128,
        TEXCOORD1 = 256,
        TEXCOORD2 = 512,
        STANDARD_ATTS = VERTEX | NORMAL | TANGENT | COLOR | TEXCOORD0,
        ALL_ATTS = VERTEX | NORMAL | NORMAL_OVERALL | TANGENT | TANGENT_OVERALL | COLOR | COLOR_OVERALL | TEXCOORD0 | TEXCOORD1 | TEXCOORD2
    };

    enum AttributeChannels : uint32_t
    {
        VERTEX_CHANNEL = 0,  // osg 0
        NORMAL_CHANNEL = 1, // osg 1
        TANGENT_CHANNEL = 2, //osg 6
        COLOR_CHANNEL = 3, // osg 2
        TEXCOORD0_CHANNEL = 4, //osg 3
        TEXCOORD1_CHANNEL = TEXCOORD0_CHANNEL + 1,
        TEXCOORD2_CHANNEL = TEXCOORD0_CHANNEL + 2,
    };


    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::vec2Array> convertToVsg(const osg::Vec2Array* inarray);

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::vec3Array> convertToVsg(const osg::Vec3Array* inarray);

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::vec4Array> convertToVsg(const osg::Vec4Array* inarray);

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::Data> convertToVsg(const osg::Array* inarray);

    extern OSG2VSG_DECLSPEC uint32_t calculateAttributesMask(const osg::Geometry* geometry);

    extern OSG2VSG_DECLSPEC VkPrimitiveTopology convertToTopology(osg::PrimitiveSet::Mode primitiveMode);

    extern OSG2VSG_DECLSPEC VkSamplerAddressMode covertToSamplerAddressMode(osg::Texture::WrapMode wrapmode);

    extern OSG2VSG_DECLSPEC std::pair<VkFilter, VkSamplerMipmapMode> convertToFilterAndMipmapMode(osg::Texture::FilterMode filtermode);

    extern OSG2VSG_DECLSPEC VkSamplerCreateInfo convertToSamplerCreateInfo(const osg::Texture* texture);

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::MaterialValue> convertToMaterialValue(const osg::Material* material);

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::Geometry> convertToVsg(osg::Geometry* geometry, uint32_t requiredAttributesMask = 0);

}
